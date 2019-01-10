// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_POLLER_EPOLLPOLLER_H
#define MUDUO_NET_POLLER_EPOLLPOLLER_H

#include <muduo/net/Poller.h>

#include <vector>

struct epoll_event;

namespace muduo
{
namespace net
{

///
/// IO Multiplexing with epoll(4).
///

/*
class Poller：实现IO multiplexing,其功能仅仅是poll系统调用的简单封装，其生命周期和EventLoop一样长，Poller不拥有Channel支持有Channel*指针(因此必要时候需要Channel自己解注册以防空悬指针)。Poller的关键数据成员是:vector<struct pollfd> pollfds_事件结构体数组用于poll的第一个参数;
map<int,Channel*>
    channels_用于文件描述符fd到Channel的映射便于快速查找到相应的Channel,如poll返回后遍历pollfds_找到就绪事件的fd再通过channels_找到fd的channel然后就可以通过Channel::handleEvent()执行就绪事件回调，值得注意的是Channel中已经记录了fd所以fd和Channel完成了双射。关键的成员函数：
    Poller::poll(int timeoutMs,vector<Channel*> activeChannels)其调用poll侦听事件集合，并在timoutMs时间内就绪的事件集合通过activeChannels返回。这里Poller::poll()在poll返回后本可以执行Channel::handleEvent()将就绪事件的回调执行了，但是并没有这样做的原因是，Channel::handleEvent()可能修改Poller的两个容器,即添加或删除Channel*，在遍历容器的时候修改容器是非常危险的，同时为了简化Poller，Poller的职责仅仅是IO复用，至于事件分发还是交给Channel自己完成。
*/
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop* loop);
    virtual ~EPollPoller();

    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels);
    virtual void updateChannel(Channel* channel);
    virtual void removeChannel(Channel* channel);

private:
    static const int kInitEventListSize = 16;

    static const char* operationToString(int op);

    void fillActiveChannels(int numEvents,
                            ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);

    typedef std::vector<struct epoll_event> EventList;

    int epollfd_;
    EventList events_;
};

}
}
#endif  // MUDUO_NET_POLLER_EPOLLPOLLER_H
