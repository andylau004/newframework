// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/base/Timestamp.h>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
/*
 *class Channel：事件分发器，其记录了描述符fd的注册事件和就绪事件，及就绪事件回调比如可读回调readCallback。其和文件描述符fd是一一对应的关系，但其不拥有fd。当一个fd想要注册事件并在事件就绪时执行相应的就绪事件回调时，首先通过Channel::update(this)->EventLoop::updateChannel(Channel*)->Poller::updateChannel(Channel*)调用链向poll系统调用的侦听事件表注册或者修改注册事件。
 * Channel作为是事件分发器其核心结构是Channel::handleEvent()该函数执行fd上就绪事件相应的事件回调，比如fd可读事件执行readCallback()。
 * Channel应该还具有一个功能是：Channel::~Channel()->EventLoop::removeChannel(Channel*)->Poller::removeChannel(Channel*)将Poller中的Channel*移除防止空悬指针。这是因为Channel的生命周期和Poller/EventLoop不一样长。
 * 其关键数据成员：fd_文件描述符，int events_文件描述符的注册事件，int revents_文件描述符的就绪事件，及事件回调readCallback_,writeCallback...
 * */
class Channel : boost::noncopyable
{
public:
    typedef boost::function<void()> EventCallback;
    typedef boost::function<void(Timestamp)> ReadEventCallback;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);
    void setReadCallback(const ReadEventCallback& cb)
    { readCallback_ = cb; }
    void setWriteCallback(const EventCallback& cb)
    { writeCallback_ = cb; }
    void setCloseCallback(const EventCallback& cb)
    { closeCallback_ = cb; }
    void setErrorCallback(const EventCallback& cb)
    { errorCallback_ = cb; }
#ifdef __GXX_EXPERIMENTAL_CXX0X__
    void setReadCallback(ReadEventCallback&& cb)
    { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback&& cb)
    { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback&& cb)
    { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback&& cb)
    { errorCallback_ = std::move(cb); }
#endif

    /// Tie this channel to the owner object managed by shared_ptr,
    /// prevent the owner object being destroyed in handleEvent.
    void tie(const boost::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // used by pollers
    // int revents() const { return revents_; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // for debug
    string reventsToString() const;
    string eventsToString() const;

    void doNotLogHup() { logHup_ = false; }

    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:
    static string eventsToString(int fd, int ev);

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;
    ///channel类的作用是把不同的IO事件分发给不同的回调函数，(一个fd上可以支持监听多个IO事件类型)
    ///每个channel对象只属于一个EventLoop
    ///每个channel对象只负责一个文件描述符fd的IO事件分发
    ///channel就像是一个容器，管理fd到callback的映射，当活动事件来临时，分发事件给不同的callback
    ///channel对象负责的fd
    const int  fd_;
    //channel关心的IO事件，有三种事件类型：kNoneEvent kReadEvent kWriteEvent，定义在channel.cc中,由enableReading disableReading enableWriting disableWriting等函数来设置
    int        events_;
    //目前活动的事件，由poller设置
    int        revents_; // it's the received event types of epoll or poll
    int        index_; // used by Poller.
    bool       logHup_;

    boost::weak_ptr<void> tie_;// 这是一个弱引用 用于对象生命期的控制 TcpConnection
    bool tied_;
    bool eventHandling_;
    bool addedToLoop_;
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
