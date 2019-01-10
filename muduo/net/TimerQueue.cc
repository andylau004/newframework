// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <muduo/net/TimerQueue.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Timer.h>
#include <muduo/net/TimerId.h>

#include <boost/bind.hpp>

#include <sys/timerfd.h>
#include <unistd.h>

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                   TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_SYSFATAL << "Failed in timerfd_create";
    }
    return timerfd;
}

// 从现在到某一个指定的时间点的时间长度
// now时刻距离when还有多长时间，返回一个timespec结构体，主要用于下面的timerfd_settime函数
struct timespec howMuchTimeFromNow(Timestamp when)
{
    // now距离when的微秒数目
    int64_t microseconds = when.microSecondsSinceEpoch()
            - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
                microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
                (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

// 从timerfd中读取8字节无意义数据，如果不读取，会一直导致poll触发
void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    // 这个读取的是使用timerfd_settime设置的值
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
    if (n != sizeof howmany)
    {
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

/* 重置定时器超时时间 */
void resetTimerfd(int timerfd, Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        LOG_SYSERR << "timerfd_settime()";
    }
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

//几个C++方面的知识点

//std::lower_bound，找到第一个大于等于给定值的位置，返回迭代器
//std::back_inserter，获取末尾位置的迭代器
//std::move，移动语义，避免拷贝，可以和右值引用一起使用
//std::bind，绑定函数指针和对象
//std::function，创建函数对象类型

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false)
{
//    LOG_INFO << "timerfd_=" << timerfd_;
    //设置timerfd可读事件回调函数为handleRead
    timerfdChannel_.setReadCallback(boost::bind(&TimerQueue::handleRead, this));
    // we are always reading the timerfd, we disarm it with timerfd_settime.
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();

    ::close(timerfd_);

    // do not remove channel, since we're in EventLoop::dtor();
    for (TimerList::iterator it = timers_.begin(); it != timers_.end(); ++it) {
        delete it->second;
    }
}

// 增加一个定时任务
TimerId TimerQueue::addTimer(const TimerCallback& cb,
                             Timestamp when,
                             double interval)
{
    // 新建一个Timer
    Timer* timer = new Timer(cb, when, interval);
    //在当前TimerQuene所属的loop中调用addTimerLoop
    // 在loop中执行真正的add操作
    loop_->runInLoop(boost::bind(&TimerQueue::addTimerInLoop, this, timer));
    // 返回一个定时任务的标示
    return TimerId(timer, timer->sequence());
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
TimerId TimerQueue::addTimer(TimerCallback&& cb,
                             Timestamp when,
                             double interval)
{
    // 右值语义
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(boost::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}
#endif

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(boost::bind(&TimerQueue::cancelInLoop, this, timerId));
}

// 在loop线程中添加timer
void TimerQueue::addTimerInLoop(Timer* timer)
{
    // 禁止跨线程
    loop_->assertInLoopThread();
    // 将timer插入至任务集合，同时返回timer是否是最早的一个任务
    bool earliestChanged = insert(timer);//如果当前插入的定时器 比队列中的定时器都早 则返回真

    //如果当前加入的timmer到时时间比原先TimerQuene中的所有定时器到时时间都短，则需要更新timefd_
    if (earliestChanged)
    {
        resetTimerfd(timerfd_, timer->expiration());//timerfd_ 重新设置超时时间
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());

    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end())
    {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1); (void)n;
        delete it->first; // FIXME: no delete please
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {
        cancelingTimers_.insert(timer);
    }

    assert(timers_.size() == activeTimers_.size());
}

// 这是Channel的回调函数，当timerfd可读触发epoll时，需要执行这个函数，执行所有的callback
void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();

    // 将计时器里数据（这个数据是通过timerfd_settime写入的）读取出来，否则会重复激发定时器
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    //获取在这个时间点到时的所有定时器，并把他们存储在expired数组中
    //同时这个函数也会把这些定时器从timers_中删除掉
    // 取出所有的到期任务
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end(); ++it)
    {
        //一次调用每个定时器的run函数，run函数里面会调用相应定时器设置的回调函数。
        it->second->run();
    }
    callingExpiredTimers_ = false;

    //因为可能有些定时器需要在一定间隔内循环触发，这里会刷新一遍定时器队列
    // 重置定时器，主要是处理那些需要重复的任务
    reset(expired, now);
}

/*
 * 重新整理时间set中的任务，将所有超时的任务都拿出，然后调用其回调函数
*/
// 取出已经到期的任务
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;// 过期的任务集合
    // 生成一个entry，用于比较，提供过期的界限
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

    /* lower_bound：找到第一个大于等于参数的位置，返回迭代器(此处如果超时时间恰好是now，应该不算作超时) */
    // 以sentry为上界，取出所有时间戳小于now的entry 此时begin和end之间，就是已经过期的任务
    // set中的任务是按照时间戳从小到大排列
    TimerList::iterator end = timers_.lower_bound(sentry);
    // 断言，要么所有任务都过期，要么now小于end结点的时间（end不属于过期的entry）
    assert(end == timers_.end() || now < end->first);

    /* back_inserter：容器适配器，将数据插入到参数的尾部 */
    // 拷贝entry
    std::copy(timers_.begin(), end, back_inserter(expired));
    // 从timers中删除这些任务
    timers_.erase(timers_.begin(), end);

    for (std::vector<Entry>::iterator it = expired.begin(); it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());
        // 从activeTimers_中删除过期任务
        size_t n = activeTimers_.erase(timer);
        assert(n == 1); (void)n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

/*
 * 调用完所有超时的回调函数后，需要对这些超时任务进行整理
 * 将周期性的定时任务重新添加到set中
 */
// 处理完任务后，需要根据是否重复，将某些任务重新放入
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (std::vector<Entry>::const_iterator it = expired.begin();
         it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());

        //如果这个定时器是需要重复启动的，即internal>0
        if (it->second->repeat()// 需要重复 而且 没有要被注销
                && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {
            it->second->restart(now);//则重启定时器，其实就是重新设置对应timer的到时时间：expiration_
            insert(it->second);//重新插入到定时器容器中
        }
        else
        {
            // 否则就将定时器删除掉，记住expired中的所有定时器已经移出timers_了
            // FIXME move to a free list
            delete it->second; // FIXME: no delete please
        }
    }

    //找到最近到时的定时器，并重新设置timefd
    if (!timers_.empty())
    {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer* timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;

    //获取这个timer的到时时间
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();

    //如果当前的定时器二叉树 times_ 为空 或 里面最邻近到时的定时器的到时时间比timer久，则设置earliestChanged
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }
    {
        //将timer加入到timers中，timers是一个set，它里面的元素按大小顺序存放
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
        assert(result.second); (void)result;
    }
    {
        //将timer加入到ActiveTimerset中
        std::pair<ActiveTimerSet::iterator, bool> result
                = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second); (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;//如果新加入的timer到时时间最近，则earliestChanged为true，反则为false
}

