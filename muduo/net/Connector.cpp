// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Connector.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG << "ctor[" << this << "]";
}
Connector::~Connector()
{
    LOG_DEBUG << "dtor[" << this << "]";
    assert(!channel_);
}

// 实际的start的函数，因为每个Channel都需要在eventloop线程执行
// 当执行函数是在别的线程执行的时候，就将要执行的函数封装成函数对象，放到 eventloop 的待执行队列中
// 然后唤醒 epoll 执行
void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);
    if (connect_)
    {
        connect();
    }
    else
    {
        LOG_DEBUG << "do not connect";
    }
}

// 停止连接的操作,同上,也是添加到eventloop 的队列中执行
void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(boost::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
    // FIXME: cancel timer
}

// 在eventloop 线程中执行的函数
void Connector::stopInLoop()
{
    loop_->assertInLoopThread();
    // 需要是在连接中,因为未开始链接的不需要停止
    if (state_ == kConnecting)
    {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno)//检查错误码
    {
    // 下面的表示可以建立连接.因此可以创建 m_channel
    case 0:// 0 表示 是由 ret 复制的 errno 没有发生错误
    case EINPROGRESS://非阻塞套接字，未连接成功返回码是EINPROGRESS表示正在连接
    case EINTR:// 被信号中断,设置的errno
    case EISCONN: //连接成功
        connecting(sockfd);// 在这些情况下，都认为会连接成功，因此下面就去构造 channel
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
        retry(sockfd);
        break;//重连

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
        LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
        sockets::close(sockfd); //这几种情况不能重连
        break;

    default:
        LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
        sockets::close(sockfd);
        // connectErrorCallback_();
        break;
    }
    LOG_INFO << "savedErrno=" << savedErrno << ", sockfd=" << sockfd;
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

// connecting() 函数，执行到这里的时候，认为，连接可以建立成功。
// 因此对 channel_ 进行赋值。 channel_ 是 scoped_ptr 因此通过 reset() 来重新赋值
// scoped_ptr 原因在于,确实只有一个连接持有这个连接
void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    assert(!channel_);

    // 新的channel,他的fd就是sockfd 是在 startinloop 中创建的
    // 这个 fd 就是后面的连接了
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(boost::bind(&Connector::handleWrite, this)); // FIXME: unsafe
    channel_->setErrorCallback(boost::bind(&Connector::handleError, this)); // FIXME: unsafe

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    // 注册到 epoll 中,
    channel_->enableWriting();
}

// 取消该Channel的监听,因为连接已经建立,这时候应该,将 sfd 传出去,给tcpconnection
int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();

    int sockfd = channel_->fd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop(boost::bind(&Connector::resetChannel, this)); // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset();
}

// 当非阻塞的连接,可写的时候,表示连接创建了 然后执行回调函数
void Connector::handleWrite()
{
    LOG_INFO << "Connector::handleWrite " << state_;

    // 能够到达这里,状态都是 kConnecting
    if (state_ == kConnecting)
    {
        // sockfd 还是 startinloop 里创建的
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err)
        {
            LOG_WARN << "Connector::handleWrite - SO_ERROR = "
                     << err << " " << strerror_tl(err);
            retry(sockfd);
        }
        else if (sockets::isSelfConnect(sockfd))
        {
            LOG_WARN << "Connector::handleWrite - Self connect";
            retry(sockfd);
        }
        else
        {
            // 没出错的情况下,那么设置 连接成功
            // 然后执行回调函数
            // 但前面可能又执行了关闭连接,所以这里要重新判断
            setState(kConnected);
            if (connect_)
            {
                newConnectionCallback_(sockfd);
            }
            else
            {
                sockets::close(sockfd);
            }
        }
    }
    else
    {
        // what happened?
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError()
{
    LOG_ERROR << "Connector::handleError state=" << state_;
    if (state_ == kConnecting)
    {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
        retry(sockfd);
    }
}

//重连函数，采用back-off策略重连，也就是退避策略
//也就是重连时间逐渐延长，0.5s,1s,2s,...一直到30s
void Connector::retry(int sockfd)
{
    // 应为 socket 在没有连接成功的情况下,一定要关闭套接字重新打开
    // 因此这里关闭,然后设置未连接.然后使用计时器
    // 指数回退,去重新连接
    // 直到,饿哦们显式的关闭连接,才停止
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_)
    {
        LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
                 << " in " << retryDelayMs_ << " milliseconds. ";

        //隔一段时间后重连，重新启用startInLoop
        loop_->runAfter(retryDelayMs_/1000.0,
                        boost::bind(&Connector::startInLoop, shared_from_this()));
        //间隔时间2倍增长
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else//超出最大重连时间后，输出连接失败
    {
        LOG_DEBUG << "do not connect";
    }
}












