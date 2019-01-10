// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Acceptor.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

// Acceptor这类对象，内部持有一个Channel，和TcpConnection相同，必须在构造函数中设置各种回调函数
// 然后在其他动作中开始监听，向epoll注册fd
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),// 创建listenfd
      acceptChannel_(loop, acceptSocket_.fd()),// 创建listenfd对应的Channel
      listenning_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))// 打开一个空的fd，用于占位
{
    assert(idleFd_ >= 0);

    acceptSocket_.setReuseAddr(true);// 复用addr
    acceptSocket_.setReusePort(reuseport);// 复用port
    acceptSocket_.bindAddress(listenAddr);// 绑定ip和port

    acceptChannel_.setReadCallback(boost::bind(&Acceptor::handleRead, this));//设置Channel的read回调函数
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

void Acceptor::listen()//构造函数和listen()执行创建TCP服务端的传统步骤 socket bind listen
{
    LOG_INFO << "Acceptor::listen is execute";
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();// 注册读事件，有读事件发生时调用handleRead()
}

void Acceptor::handleRead()//当epoll监听到listenfd时，开始执行此回调函数
{
    loop_->assertInLoopThread();

    InetAddress peerAddr;
    //FIXME loop until no more
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
//        string hostport = peerAddr.toIpPort();
//        LOG_TRACE << "Accepts of " << hostport;
        //这里的回调函数 newConnectionCallback 是在 Acceptor::setNewConnectionCallback(newConnectionCallback) 指定的，
        //TcpServer构造时 new 一个 Acceptor 后，会通过这个函数指定回调函数为 TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
        //接受完连接后回调 newConnectionCallback_ 传回connfd，创建TcpConnection 再将连接分配给其他线程
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);// 这里调用TcpServer::newConnection()函数
        }
        else {
            sockets::close(connfd);
        }
    }
    else
    {
        LOG_SYSERR << "in Acceptor::handleRead";
        // Read the section named "The special problem of
        // accept()ing when you can't" in libev's doc.
        // By Marc Lehmann, author of libev.

        /*
         * 本进程的fd达到上限后无法为新连接创建socket描述符
         * 既然没有socketfd来表示这个连接，也就无法close它
         * 进程继续运行，下次epoll_wait会直接返回，因为listenfd还是可读
         * 这样进程就陷入了 busy loop
         *
         * 处理fd 满的时候，采用了这样一种方法
         * 就是先占住一个空的fd，然后当fd满的时候
         * 先关闭这个空闲文档，获得一个文档描述符的名额
         * 再调用accept拿到新socket连接的描述符
         * 随后立即close调它，这样就优雅地断开了客户端连接
         * 最后重新打开一个空闲文档，把"坑"占住，以备情况再发生
         */

        // 这里处理fd达到上限有一个技巧，就是先占住一个空的fd，然后当fd满的时候，先关闭此占位fd，然后
        // 迅速接受新的tcp连接，然后关闭它，然后再次打开此fd
        // 这样的好处是能够及时通知客户端，服务器的fd已经满。
        // 事实上，这里还可以提供给用户一个回调函数，提供fd满时的更具体信息
        if (errno == EMFILE)//fd的数目达到上限
        {
            ::close(idleFd_);//关闭占位fd
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);//接受这个连接
            ::close(idleFd_);//关掉
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);//重新打开此fd 占位
        }
    }
}

