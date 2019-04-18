// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThreadPool.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
    // Don't delete loop, it's stack variable
    // 这里无需delete loop，因为他们都是栈上的对象，
    // 从EventLoopThread的源码中可以看到，loop是一个局部对象，
    // 无须手工delete。
    // 注意这个线程池如果析构：
    // 1.成员对象threads_过期，需要析构
    // 2.threads_析构导致内部的每个EventLoopThread被析构
    // 3.每个线程内的EventLoop被执行loop
    // 4.每个线程被join
    // 5.所有的EventLoop以及所在线程全部销毁
    // 6.本线程池不需要做任何任务
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    assert(!started_);
    // 禁止跨线程，必须在baseLoop所在线程内开启线程池
    baseLoop_->assertInLoopThread();

//    printf("cb=%p\n", &cb);
    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);

        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(t);

        // 开启线程，开启EventLoop
        loops_.push_back(t->startLoop());
    }
    // 如果线程数目为0，那么执行回调函数
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

// 这是一个调度算法，用于master Loop向线程池内的Loop分派任务
// 如果无loop可以调度，那么只好向baseLoop，也就是向自身添加任务
EventLoop* EventLoopThreadPool::getNextLoop()
{
    baseLoop_->assertInLoopThread();
    assert(started_);
    EventLoop* loop = baseLoop_;

    if (!loops_.empty())
    {
        // round-robin
        loop = loops_[next_];
        ++next_;
        if (implicit_cast<size_t>(next_) >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}

// 这也是一个调度算法，只是这里采用的不是轮转，而是hash
// hash算法可以由外部执行
EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode)
{
    baseLoop_->assertInLoopThread();
    EventLoop* loop = baseLoop_;

    if (!loops_.empty())
    {
        loop = loops_[hashCode % loops_.size()];
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    baseLoop_->assertInLoopThread();
    assert(started_);
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}
