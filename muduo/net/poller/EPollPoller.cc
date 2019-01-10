// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/EPollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
BOOST_STATIC_ASSERT(EPOLLIN == POLLIN);
BOOST_STATIC_ASSERT(EPOLLPRI == POLLPRI);
BOOST_STATIC_ASSERT(EPOLLOUT == POLLOUT);
BOOST_STATIC_ASSERT(EPOLLRDHUP == POLLRDHUP);
BOOST_STATIC_ASSERT(EPOLLERR == POLLERR);
BOOST_STATIC_ASSERT(EPOLLHUP == POLLHUP);

namespace
{
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)
{
    LOG_INFO << "epollfd_=" << epollfd_;

    if (epollfd_ < 0)
    {
        LOG_SYSFATAL << "EPollPoller::EPollPoller";
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    char tmpbuffer[ 4098 ];
    char* pPtr = tmpbuffer;
    for ( ChannelMap::iterator itKv = channels_.begin(); itKv != channels_.end(); itKv ++ ) {
        sprintf( pPtr, "channelfd=%d ", itKv->first );
        pPtr += strlen(pPtr);
    }

//    LOG_INFO << "fd total count " << channels_.size();
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
//    LOG_INFO << "tmpbuffer=" << tmpbuffer << ", numEvents=" << numEvents;// << ", curtid=" << CurrentThread::tid();

    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
//        LOG_INFO << numEvents << " events happened";
        fillActiveChannels(numEvents, activeChannels); //调用fillActiveChannels，传入numEvents也就是发生的事件数目
        if (implicit_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size()*2); //如果返回的事件数目等于当前事件数组大小，就分配2倍空间
        }
    }
    else if (numEvents == 0) {
        LOG_TRACE << "nothing happened";
    }
    else {
        // error happens, log uncommon ones
        if (savedErrno != EINTR)
        {
            errno = savedErrno;
            LOG_SYSERR << "EPollPoller::poll()";
        } else {
            LOG_WARN << "savedErrno=" << savedErrno;
        }
    }
    return now;
}

void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
    assert(implicit_cast<size_t>(numEvents) <= events_.size());
    for (int i = 0; i < numEvents; ++i)//挨个处理发生的numEvents个事件，epoll模式返回的events_数组中都是已经发生de事件，这有别于select和poll
    {
/*
        这是epoll模式epoll_event事件的数据结构，其中data不仅可以保存fd，也可以保存一个void*类型的指针。
        typedef union epoll_data {
                       void    *ptr;
                       int      fd;
                       uint32_t u32;
                       uint64_t u64;
                   } epoll_data_t;
                   struct epoll_event {
                       uint32_t     events;    // Epoll events
                       epoll_data_t data;      //User data variable
                   };
*/
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);

#ifndef NDEBUG
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
#endif

//        LOG_INFO << "activechannelfd=" << channel->fd() << ", events_[" << i << "].events=" << events_[i].events;
        channel->set_revents(events_[i].events); //把已发生的事件传给channel,写到通道当中
        activeChannels->push_back(channel);
    }
}

void EPollPoller::updateChannel(Channel* channel)
{
    Poller::assertInLoopThread();
    const int index = channel->index();
    LOG_TRACE << "fd = " << channel->fd()
              << " events = " << channel->events() << " index = " << index;
    if (index == kNew || index == kDeleted)//说明这个Channel目前没有被epoll监听
    {
        // a new one, add with EPOLL_CTL_ADD
        int fd = channel->fd();//获取Channel中的文件描述符
        if (index == kNew)//如果这个Channel是最新的，也就是之前没有被加进来过
        {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        }
        else // index == kDeleted //说明这个channel以前被加进来过，此时说明channels数组中已经存储了对应的channel
        {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }

        channel->set_index(kAdded);//设置channel的index为kAdded
        update(EPOLL_CTL_ADD, channel);//将监听事件的描述符加入到epoll中
    }
    else//如果index==kAdded，说明这个channel已经加进来了，并且epoll已经在监听这个channel中的描述符了
    {
        // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->fd();
        (void)fd;
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);
        if (channel->isNoneEvent())//如果channel中已经没有想监听的事件，则将这个channel的fd从epoll中删除
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else//另一种调用这个函数的情况是，修改监听事件的类型
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel)
{
    Poller::assertInLoopThread();
    int fd = channel->fd();
    LOG_TRACE << "fd = " << fd;

    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());

    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    size_t n = channels_.erase(fd);
    (void)n;
    assert(n == 1);

    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel)
{
    struct epoll_event event;
    bzero(&event, sizeof event);
    event.events   = channel->events();
    event.data.ptr = channel;

    int fd = channel->fd();
    LOG_TRACE << "epoll_ctl op = " << operationToString(operation)
              << " fd = " << fd << " event = { " << channel->eventsToString() << " }";

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
        }
        else
        {
            LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
        }
    }
}

const char* EPollPoller::operationToString(int op)
{
    switch (op)
    {
    case EPOLL_CTL_ADD:
        return "ADD";
    case EPOLL_CTL_DEL:
        return "DEL";
    case EPOLL_CTL_MOD:
        return "MOD";
    default:
        assert(false && "ERROR op");
        return "Unknown Operation";
    }
}
