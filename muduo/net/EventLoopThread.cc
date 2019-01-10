// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
#include <stdio.h>
#include <iostream>

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

#include "muduo/base/Logging.h"

using namespace muduo;
using namespace muduo::net;


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
    : loop_(NULL),
      exiting_(false),
      thread_(boost::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(mutex_),
      callback_(cb)
{
    //    printf( "callback_ addr: %p\n", &callback_ );
}

// 此EventLoop的退出流程：
// 1.EventLoopThread对象过期
// 2.如果loop_不为NULL，说明正在执行（因为loop初始为NULL，一旦运行起来，就再也不是NULL）
// 需要将内部的EventLoop关闭
// 3.join线程
EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
    {
        LOG_DEBUG << "loop_ != Null";
        // still a tiny chance to call destructed object, if threadFunc exits just now.
        // but when EventLoopThread destructs, usually programming is exiting anyway.
        loop_->quit();
        thread_.join();
    }
}

// 开启线程，并且返回持有的EventLoop的指针
EventLoop* EventLoopThread::startLoop()
{
    assert(!thread_.started());
    thread_.start();

    // 这里存在一个race Condition
    // 线程刚刚启动时，里面的EventLoop的指针还没有赋给loop_，所以
    // 这里需要等待，当指针赋值成功后，cond_会通知这里继续执行
    {
        MutexLockGuard lock(mutex_);
        while (loop_ == NULL)
        {
            cond_.wait();
        }
    }

    return loop_;
}

//该函数是EventLoopThread类的核心函数，作用是启动loop循环
//该函数和上面的startLoop函数并发执行，所以需要上锁和condition
void EventLoopThread::threadFunc()
{
    EventLoop loop;

    if (callback_)
    {
        callback_(&loop); //构造函数传递进来的，线程启动执行回调函数
    }

    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop; //然后loop_指针指向了这个创建的栈上的对象，threadFunc退出之后，这个指针就失效了
        cond_.notify(); //该函数退出，意味着线程就推出了，EventLoopThread对象也就没有存在的价值了。但是muduo的EventLoopThread
        //实现为自动销毁的。一般loop函数退出整个程序就退出了，因而不会有什么大的问题，
        //因为muduo库的线程池就是启动时分配，并没有释放。所以线程结束一般来说就是整个程序结束了。
    }

//    printf( "EventLoopThread obj: %p tmp loop addr: %p\n", this, &loop );
    loop.loop();
    //assert(exiting_);
    loop_ = NULL;
}

