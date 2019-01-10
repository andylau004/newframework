// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
#include <stdio.h>
#include <ostream>

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
// 线程局部变量，实质是线程内部的全局变量
// 这个变量记录本线程持有的EventLoop的指针
// 一个线程最多持有一个EventLoop，所以创建EventLoop时检查该指针即可
__thread EventLoop* t_loopInThisThread = 0;

// poll或epoll调用的超时事件，默认10s
const int kPollTimeMs = -1/*10000*/;

int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_SYSERR << "Failed in eventfd";
        abort();
    }
    return evtfd;
}

// 下面的代码是为了屏蔽SIGPIPE信号
// 对于一条已经关闭的tcp连接，第一次发数据收到RST报文，第二次发收到SIGPIPE信号
// 默认是终止进程
#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
public:
    IgnoreSigPipe()
    {
        ::signal(SIGPIPE, SIG_IGN);
        // LOG_TRACE << "Ignore SIGPIPE";
    }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;// 这里利用C++的全局对象，在main函数调用前，就屏蔽了SIGPIPE
}

// 返回本线程持有的EventLoop指针
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
    return t_loopInThisThread;
}


EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      iteration_(0),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(NULL)
{
    LOG_INFO << "wakeupFd_=" << wakeupFd_;

//    LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
    // 如果t_loopInThisThread不为空，那么说明本线程已经开启了一个EventLoop
    if (t_loopInThisThread)
    {
        LOG_FATAL << "Another EventLoop " << t_loopInThisThread
                  << " exists in this thread " << threadId_;
    }
    else
    {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(boost::bind(&EventLoop::handleRead, this));
    // we are always reading the wakeupfd
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
              << " destructs in thread " << CurrentThread::tid();

    wakeupChannel_->disableAll();
    wakeupChannel_->remove();

    ::close(wakeupFd_);
    t_loopInThisThread = NULL;
}

//事件循环,该函数不能跨线程调用，只能在创建该对象的线程中调用
/*
 * IO线程平时阻塞在事件循环EventLoop::loop()的poll调用中，为了让IO线程能立刻执行用户回调，需要唤醒它。
 * 传统的方法是使用管道pipe，IO线程始终监视此管道的readable事件，在需要唤醒的时候，其他线程往管道里写一个字节，这样IO线程就从IO多路复用阻塞调用中返回。
 * linux中使用eventfd，可以更高效地唤醒，因为它不必管理缓冲区。
 * eventfd 是一个比 pipe 更高效的线程间事件通知机制，一方面它比 pipe 少用一个 file descripor，节省了资源；
 * 另一方面，eventfd 的缓冲区管理也简单得多，全部“buffer” 只有定长8bytes，不像 pipe 那样可能有不定长的真正 buffer。
 */
void EventLoop::loop()
{
    assert(!looping_);// 禁止重复开启loop
    assertInLoopThread();// 禁止跨线程
    looping_ = true;
    quit_    = false;  // FIXME: what if someone calls quit() before loop() ?
    LOG_TRACE << "EventLoop " << this << " start looping";

    while (!quit_)
    {
        // 每次poll调用，就是一次重新填充activeChannels_的过程 所以这里需要清空
        activeChannels_.clear();
        // 调用poll获得当前活动事件的channel列表（其实是将有活动事件的fd对应的channel填入activechannels_），
        // 然后依次调用每个channel的handleEvent函数 这一步的实质是进行poll或者epoll_wait调用
        // 根据fd的返回事件，填充对应的Channel，以准备后面执行回调处理事件
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;

        if (Logger::logLevel() <= Logger::TRACE) {
            printActiveChannels();
        }
        if ( activeChannels_.size() ) {
//            LOG_INFO << "activeChannels_size=" << activeChannels_.size();
        }

        // TODO sort channel by priority
        eventHandling_ = true;
        for (ChannelList::iterator it = activeChannels_.begin(); it != activeChannels_.end(); ++it) {
            currentActiveChannel_ = *it;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = NULL;
        eventHandling_ = false;

        //执行等待队列中的回调函数
        doPendingFunctors();
    }

    LOG_TRACE << "EventLoop " << this << " stop looping";
    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;
    // There is a chance that loop() just executes while(!quit_) and exits,
    // then EventLoop destructs, then we are accessing an invalid object.
    // Can be fixed using mutex_ in both places.
    if (!isInLoopThread())
    {
        LOG_DEBUG << "!IsInloopThread" << ", threadId_=" << threadId_;// << ", curtid=" << CurrentThread::tid() ;
        wakeup();
    }
}

/*
1、不是简单地在临界区内依次调用Functor，而是把回调列表swap到functors中，
这样一方面减小了临界区的长度（意味着不会阻塞其它线程的queueInLoop()），
另一方面，也避免了死锁（因为Functor可能再次调用queueInLoop()）

2、由于doPendingFunctors()调用的Functor可能再次调用queueInLoop(cb)，
这时，queueInLoop()就必须wakeup()，否则新增的cb可能就不能及时调用了

3、muduo没有反复执行doPendingFunctors()直到pendingFunctors_为空而是每次poll返回就执行一次，
这是有意的，否则IO线程可能陷入死循环，无法处理IO事件。
*/
void EventLoop::queueInLoop(const Functor& cb)
{
    //这里增加业务逻辑是增加执行任务的函数指针的，增加的任务保存在成员变量pendingFunctors_中，
    //这个变量是一个函数指针数组（vector对象），执行的时候，调用每个函数就可以了。
    //上面的代码先利用一个栈变量将成员变量pendingFunctors_里面的函数指针换过来，接下来对这个栈变量进行操作就可以了，
    //这样减少了锁的粒度。因为成员变量pendingFunctors_在增加任务的时候，也会被用到，设计到多个线程操作，所以要加锁，增加任务的地方是：
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(cb);
    }
    /*
     * runInLoop的实现：需要使用eventfd唤醒的两种情况 (1) 调用queueInLoop的线程不是当前IO线程。(2)是当前IO线程并且正在调用pendingFunctor。
    */
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        char tmpout[ 2048 ] = {0};
        sprintf( tmpout, "queueInLoop trig Functor& cb isInLoopThread: %d eventloopTid_: %d callingPendingFunctors_: %d",
                 isInLoopThread(), threadId_, //CurrentThread::tid(),
                 callingPendingFunctors_ );
//        LOG_INFO << tmpout;
        wakeup();
    }
}

size_t EventLoop::queueSize() const
{
    MutexLockGuard lock(mutex_);
    return pendingFunctors_.size();
}

TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
    return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(cb, time, interval);
}

/*
 * 1.如果事件循环不属于当前这个线程，就不能直接调用回调函数，应该回到自己所在线程调用
 * 2.此时需要先添加到自己的队列中存起来，然后唤醒自己所在线程的io复用函数（poll）
 * 3.唤醒方法是采用eventfd，这个eventfd只有8字节的缓冲区，向eventfd中写入数据另poll返回
 * 4.返回后会调用在队列中的函数，见EventLoop
 *
 * 举例说明什么时候会出现事件驱动循环不属于当前线程的情况
 *      1.客户端close连接，服务器端某个Channel被激活，原因为EPOLLHUP
 *      2.Channel调用回调函数，即TcpConnection的handleClose
 *      3.handleClose调用TcpServer为它提供的回调函数removeConnection
 *      4.此时执行的是TcpServer的removeConnection函数，
 * 解释
 *      1.因为TcpServer所在线程和TcpConnection所在的不是同一个线程
 *      2.这就导致将TcpServer暴露给了TcpConnection所在线程
 *      3.因为TcpServer需要将这个关闭的TcpConnection从tcp map中删除
 *        就需要调用自己的另一个函数removeConnectionInLoop
 *      4.为了实现线程安全性，也就是为了让removeConnectionInLoop在TcpServer自己所在线程执行
 *        需要先把这个函数添加到队列中存起来，等到回到自己的线程在执行
 *      5.runInLoop中的queueInLoop就是将这个函数存起来
 *      6.而此时调用runInLoop的仍然是TcpConnection所在线程
 *      7.因为自始至终，removeConnection这个函数都还没有结束
 *
 * 如果调用runInLoop所在线程和事件驱动循环线程是一个线程，那么直接调用回调函数就行了
 *
 * 在TcpServer所在线程中，EventLoop明明阻塞在poll上，这里为什么可以对它进行修改
 *      1.线程相当于一个人可以同时做两件事情，一个EventLoop同时调用两个函数就很正常了
 *      2.其实函数调用都是通过函数地址调用的，既然EventLoop可读，就一定直到内部函数的地址，自然可以调用
 *      3.而更改成员函数，通过地址访问，进而修改，也是可以的
 */
void EventLoop::runInLoop(const Functor& cb)
{
    if (isInLoopThread())
    {
//        LOG_INFO << ( "aaaaaaaaaaaaaaaaaaaaaaaaaa" );
        cb();
    }
    else
    {
        LOG_INFO << ( "bbbbbbbbbbbbbbbbbbbbbbbbbb" );
        queueInLoop(cb);
    }
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
// FIXME: remove duplication
//void EventLoop::runInLoop(Functor&& cb)
//{
//    if (isInLoopThread()) {
//        LOG_INFO << ( "new aaaaaaaaaaaaaaaaaaaaaaaaaa" );
//        cb();
//    }
//    else {
//        LOG_INFO << ( "new bbbbbbbbbbbbbbbbbbbbbbbbbb" );
//        queueInLoop(std::move(cb));
//    }
//}
//void EventLoop::queueInLoop(Functor&& cb)
//{
//    {
//        MutexLockGuard lock(mutex_);
//        pendingFunctors_.push_back(std::move(cb));  // emplace_back
//    }
//    if (!isInLoopThread() || callingPendingFunctors_) {
//        char tmpout[ 2048 ] = {0};
//        sprintf( tmpout, "queueInLoop trig Functor&& cb isInLoopThread: %d eventloopTid_: %d  callingPendingFunctors_: %d",
//                 isInLoopThread(), threadId_, /*CurrentThread::tid(),*/ callingPendingFunctors_ );
//        LOG_INFO << tmpout;
//        wakeup();
//    }
//}

//TimerId EventLoop::runAt(const Timestamp& time, TimerCallback&& cb)
//{
//    return timerQueue_->addTimer(std::move(cb), time, 0.0);
//}

//TimerId EventLoop::runAfter(double delay, TimerCallback&& cb)
//{
//    Timestamp time(addTime(Timestamp::now(), delay));
//    return runAt(time, std::move(cb));
//}

//TimerId EventLoop::runEvery(double interval, TimerCallback&& cb)
//{
//    Timestamp time(addTime(Timestamp::now(), interval));
//    return timerQueue_->addTimer(std::move(cb), time, interval);
//}
#endif

void EventLoop::cancel(TimerId timerId)
{
    return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

// 移除Channel
void EventLoop::removeChannel(Channel* channel)
{
    // 该Channel必须位于本EventLoop内
    assert(channel->ownerLoop() == this);
    // 禁止跨线程
    assertInLoopThread();
    if (eventHandling_)
    {
        assert(currentActiveChannel_ == channel ||
               std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
    }
    // 从poller中移除该fd，停止对该fd的监听
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

// 当跨线程进行一些不允许跨线程的操作时，打印出错误信息，并挂掉程序
void EventLoop::abortNotInLoopThread()
{
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << threadId_
              << ", current thread id = " <<  CurrentThread::tid();
}

// 每个EventLoop内部持有了一个eventfd包装的Channel
// 用来激活poll调用
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

// 处理eventfd的读事件，也是对应Channel的读事件回调函数
void EventLoop::handleRead()
{
    uint64_t one = 1;
    // 从eventfd中读出8个字节，防止poll被重复触发
    ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        // 这里使用了缩减临界区代码的技巧，减少锁的争用
        // 这里之所以会出现并发的问题，是因为本函数不会跨线程
        // 但是runInLoop可以跨函数，会更改pendingFunctors_
        // 所以这里需要加锁
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    char tmpout[ 2048 ] = { 0 };
    if ( functors.size() )
        sprintf( tmpout, "doPendingFunctors work functors_size=%ld", functors.size() );
    else
        sprintf( tmpout, "doPendingFunctors work size==0" );
//    LOG_INFO << tmpout;

    for (size_t i = 0; i < functors.size(); ++i) {
        functors[i]();
    }

    // callingPendingFunctors_是个标示，表示EventLoop是否在处理任务
    callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
    for (ChannelList::const_iterator it = activeChannels_.begin(); it != activeChannels_.end(); ++it) {
        const Channel* ch = *it;
        LOG_TRACE << "{" << ch->reventsToString() << "} ";
    }
}

