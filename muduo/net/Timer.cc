// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Timer.h>

using namespace muduo;
using namespace muduo::net;

AtomicInt64 Timer::s_numCreated_;

void Timer::restart(Timestamp now)
{
    //如果需要重复，那就将时间设为下次超时的时间
    if (repeat_)
    {
        expiration_ = addTime(now, interval_);
    }
    else
    {
        //如果不需要重复，那就将超时时间设为一个不可用的 value
        expiration_ = Timestamp::invalid();
    }
}
