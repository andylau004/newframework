// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
    : loop_(CHECK_NOTNULL(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(boost::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for (ConnectionMap::iterator it(connections_.begin()); it != connections_.end(); ++it)
    {
        TcpConnectionPtr conn(it->second);
        it->second.reset();
        conn->getLoop()->runInLoop(boost::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (started_.getAndSet(1) == 0)
    {
        threadPool_->start(threadInitCallback_);

        assert(!acceptor_->listenning());
        loop_->runInLoop(boost::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    loop_->assertInLoopThread();

    EventLoop* ioLoop = threadPool_->getNextLoop();
    LOG_INFO << "post ioLoop->threadId_=" << ioLoop->IoEventLoopTid();// << ", CurTid=" << CurrentThread::tid();

    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_); //端口+连接id
    ++nextConnId_;//++之后就是下一个连接id
    string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << peerAddr.toIpPort() << ", fd=" << sockfd;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    // TcpConnection的use_count此处为1，新建了一个Tcpconnection
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
//    LOG_INFO << "new one conn use_count=" << conn.use_count();
    connections_[connName] = conn;
    //TcpConnection的use_count此处为2，因为加入到connections_中
    LOG_INFO << "insert into map use_count=" << conn.use_count();

    //实际TcpServer的connectionCallback等回调函数是对conn的回调函数的封装，所以在这里设置过去
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //将TcpServer的removeConnection设知道了TcpConnection的关闭回调函数中
    conn->setCloseCallback(boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe

    LOG_INFO << "before runInLoop new conn use_count=" << conn.use_count();
    //调用TcpConenction::connectEstablished函数内部会将use_count加一然后减一，此处仍为2
    //但是本函数介绍结束后conn对象会析构掉，所以引用计数为1，仅剩connections_列表中存活一个
    ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));
    LOG_INFO << "after  runInLoop new conn use_count=" << conn.use_count();
}

/*
是因为简单地断开一个connction涉及到多个类的操作，你必须要通过runInLoop到那个类中，进行相应操作，且顺序不能乱

A. 将conn从connctions里面移除

B. 将connChannel从Loop中的监听队列中remove

C. 在A，B的基础上你又要保证conn的生命周期，使得conn存活到B之后
*/
void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    // FIXME: unsafe
    loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));
}
// muduo被动关闭tcp连接的流程：
// 1. read收到0，或者epoll监听到HUP事件
// 2. 调用conn中的handleClose函数
// 3. 停止监听所有的事件
// 4. 执行用户的close逻辑
// 5. 执行close回调函数：
// 6. 执行TcpServer中的removeConnection（removeConnectionInLoop）
// 7. connections_中移除conn，引用计数-1
// 8. 执行TcpTcpConnection中connectDestroyed，将Channel指针从loop中移除
// 在上述关闭过程中，为什么需要用到TcpServer中的函数，原因是connections_这个数据结构的存在
// 为了维持TcpConnection的生存期，需要将ptr保存在connections_中，当tcp关闭时，也必须去处理这个数据结构
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
             << "] - connection " << conn->name();

    // 根据conn的name，从map容器中删除，此时引用计数会减1。
    // erase之前引用计数为2（由前面的shared_from_this()保证），所以执行完erase，引用计数变为1
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);

    // 此处一定要用EventLoop::queueInLoop()，避免Channel对象被提前销毁
    // 这里用boost::bind让TcpConnection的生命期长到调用connectDestroyed()的时刻
    // 使用boost::bind得到一个boost::function对象,会把conn传递进去，引用计数会加1
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(boost::bind(&TcpConnection::connectDestroyed, conn));
    // bind延长了conn的生命期，connectDestroyed完成后，TcpConnection被析构
}

