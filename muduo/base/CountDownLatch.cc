// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <stdio.h>
#include <muduo/base/CountDownLatch.h>


using namespace muduo;


CountDownLatch::CountDownLatch(int count)
    : mutex_(), condition_(mutex_), count_(count)
{
//    printf( "countdownlatch cst this=%p\n", this );
}
CountDownLatch::~CountDownLatch() {
//    printf( "countdownlatch this=%p\n", this );
}

// latchdown之后然后调用wait.等待最后一个线程notifyAll,
// 然后多个线程同时解除锁定就可以同时开始执行了
void CountDownLatch::wait()
{
    MutexLockGuard lock(mutex_);
    while (count_ > 0)
    {
        condition_.wait();
    }
}
// 每个线程启动成功后就执行latchdown,最后一个线程会notifyAll
void CountDownLatch::countDown()
{
    MutexLockGuard lock(mutex_);
    --count_;
    if (count_ == 0)
    {
        condition_.notifyAll();
    }
}

int CountDownLatch::getCount() const
{
    MutexLockGuard lock(mutex_);
    return count_;
}

