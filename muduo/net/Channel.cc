// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

// Poll与EPoll中的事件相同，
// POLLIN与EPOLLIN POLLPRI与EPOLLPRI POLLOUT与EPOLLOUT等价
// 这里可以加上静态断言来确定
const int Channel::kNoneEvent = 0;// 没有需要监听的事件
const int Channel::kReadEvent = POLLIN | POLLPRI;// 读事件
const int Channel::kWriteEvent = POLLOUT;// 写事件

Channel::Channel(EventLoop* loop, int fd__)
    : loop_(loop),
      fd_(fd__),
      events_(0),
      revents_(0),
      index_(-1),
      logHup_(true),
      tied_(false),
      eventHandling_(false),
      addedToLoop_(false)
{
}

// 析构函数中会判断Channel是否仍处于事件处理状态
// 在事件处理期间Channel对象不会析构
// 析构时，必须确保loop中已经移除了该fd，或者epoll中已经删除了该fd
Channel::~Channel()
{
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread())
    {
        assert(!loop_->hasChannel(this));
    }
}

//从shared_ptr对象obj构造weak_ptr tie_，获得资源的观测权
//防止shared_ptr出现循环引用
/*
 * 1. 当连接到来，创建一个TcpConnection对象，立刻用shared_ptr来管理，引用计数为1，
 * 在Channel中维护一个weak_ptr（tie_），将这个shared_ptr对象赋值给_tie，引用计数仍然为1。
 * 2. 当连接关闭时，在handleEvent中，将tie_提升，得到一个shard_ptr对象，引用计数就变成了2。
 * 当shared_ptr的计数不为0时，TcpConnection不会被销毁。
*/
void Channel::tie(const boost::shared_ptr<void>& obj)
{
    tie_  = obj;
    tied_ = true;
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}
// 同上，通过EventLoop调用Epoller中函数，达到从epoll或者poll中删除fd的目的
// 进行该操作时，必须确保该Channel已经不再监听任何事件
void Channel::remove() {
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

//handleEvent是channel的核心,由EventLoop::loop()通过channel对象调用，然后该函数根据revents_的值分别调用不同的用户回调
void Channel::handleEvent(Timestamp receiveTime) {
    boost::shared_ptr<void> guard;
    if (tied_)
    {
        //lock从被观测的shared_ptr获得一个可用的shared_ptr对象，从而操作资源
        //handleEventWithGuard会进行recv处理，POLLIN可读就调用Channel之前注册的读回调函数，此外还有写回调，关闭回调等，这些都是TcpServer注册到TcpConnection在关联到TcpConnection所拥有的已连接套接字的，通过执行newConnection()函数之时，你可以回头看看。
        //可见，处理连接事件时，对TcpConnection的智能指针进行了提升，因为已连接套接字fd的生命期是由TcpConnection管理的，我们要确保处理事件时该对象还存活，否则使用一个已经死亡的对象，结果只有core dump。
        guard = tie_.lock();// 这里是对弱指针的一个提升
        if (guard)
        {
//            LOG_INFO << "tie_ handleEvent";
            handleEventWithGuard(receiveTime);// 调用提前注册的回调函数处理读写事件
        }
    }
    else
    {
//        LOG_INFO << "no tie_ handleEvent";
        handleEventWithGuard(receiveTime);
    }
}

// 根据revents_的值(目前活动的事件)分别调用不同的用户回调，也就是说channel对象处理fd上各种类型的事件，与events_无关(?)
// 这里利用了Channel对象的“多态性”，
// 如果是普通socket，可读事件就会调用预先设置的回调函数；
// 如果是侦听socket，则调用Aceptor对象的handleRead()
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    eventHandling_ = true;
    LOG_TRACE << reventsToString();
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))// 判断返回的事件 为挂断 close
    {
        LOG_INFO << "revents_=" << revents_ << " closeCallback_";
        if (logHup_)
        {
            LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
        }
        if (closeCallback_) closeCallback_();
    }

    if (revents_ & POLLNVAL)// 文件描述符fd没打开或者非法
    {
        LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
    }

    if (revents_ & (POLLERR | POLLNVAL))// 错误的
    {
        LOG_INFO << "revents_=" << revents_ << " errorCallback_";
        if (errorCallback_) errorCallback_();
    }
    // POLLRDHUP 关闭连接或者关闭半连接
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))// 对等放调用close关闭连接 会受到POLLRDHUP POLLPRI(带外数据)
    {
        // 当是侦听socket时，readCallback_指向Acceptor::handleRead
        // 当是客户端socket时，调用TcpConnection::handleRead
//        LOG_INFO << "revents_=" << revents_ << " readCallback_";
        if (readCallback_) readCallback_(receiveTime);// 产生可读事件 调用读函数
    }
    if (revents_ & POLLOUT)
    {
        //如果是连接状态服的socket，则writeCallback_指向Connector::handleWrite()
//        LOG_INFO << "revents_=" << revents_ << " writeCallback_";
        if (writeCallback_) writeCallback_();// 可写事件的产生 调用写的回调函数
    }
    eventHandling_ = false;
}

string Channel::reventsToString() const
{
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
        oss << "IN ";
    if (ev & POLLPRI)
        oss << "PRI ";
    if (ev & POLLOUT)
        oss << "OUT ";
    if (ev & POLLHUP)
        oss << "HUP ";
    if (ev & POLLRDHUP)
        oss << "RDHUP ";
    if (ev & POLLERR)
        oss << "ERR ";
    if (ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str().c_str();
}
