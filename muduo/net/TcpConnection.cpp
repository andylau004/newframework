// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpConnection.h>

#include <muduo/base/Logging.h>
#include <muduo/base/WeakCallback.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Socket.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;


/*
 * 发送数据使用的是写缓冲区，当内核tcp缓冲区空间不足时，会把数据写到写缓冲区，由写缓冲区在合适的时机写入内核tcp缓冲区，合适的时机指内核tcp缓冲区有多余空间时。
但是怎样才能直到内核tcp缓冲区有多余的空间呢，通过监听可写事件即可。
但是如果内核tcp缓冲区一直不满，那么就一直可写，就会一直触发poll，导致busy loop，所以muduo只有在需要的时候才会检测内核tcp缓冲区的可写事件，即只有当tcp缓冲区已满，但是写缓冲区中有数据等待写入tcp缓冲区时才会监听。

不同于读取数据的是，发送数据使用的是TcpConnection提供的接口，而不是直接向Buffer中写。
*/

void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
              << conn->peerAddress().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWN");
    // do not call conn->forceClose(), because some users want to register message callback only.
}

void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{
    buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop* loop,
                             const string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(CHECK_NOTNULL(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64*1024*1024)
{
    channel_->setReadCallback(
                boost::bind(&TcpConnection::handleRead, this, _1));
    //如果tcp缓冲区不足以全部容纳数据，就会开启对可写事件的监听，当tcp缓冲区可写，就调用Channel的回调函数，这个回调函数也是在TcpConnection构造函数中传给Channel的
    channel_->setWriteCallback(
                boost::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
                boost::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
                boost::bind(&TcpConnection::handleError, this));
    LOG_INFO << "TcpConnection::ctor[" <<  name_ << "] at " << this << " fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO  << "TcpConnection::dtor[" <<  name_ << "] at " << this
              << " fd=" << channel_->fd()
              << " state=" << stateToString();
    assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
{
    return socket_->getTcpInfo(tcpi);
}

string TcpConnection::getTcpInfoString() const
{
    char buf[1024];
    buf[0] = '\0';
    socket_->getTcpInfoString(buf, sizeof buf);
    return buf;
}

void TcpConnection::send(const void* data, int len)
{
    send(StringPiece(static_cast<const char*>(data), len));
}

void TcpConnection::send(const StringPiece& message)
{
    if (state_ == kConnected)
    {
        /*
         * 如果当前线程和TcpConnection所属线程相同，直接在当前线程发送
         * 否则，需要使用std::bind绑定函数和对象，并添加到自己所在线程的事件循环中
         */
        if (loop_->isInLoopThread())
        {
//            LOG_INFO << "send message in loop";
            sendInLoop(message);
        }
        else
        {
            LOG_INFO << "send message no in loop";
            loop_->runInLoop(
                        boost::bind(&TcpConnection::sendInLoop,
                                    this,     // FIXME
                                    message.as_string()));
            //std::forward<string>(message)));
        }
    }
}

// FIXME efficiency!!!

// 情景一: 如果 TcpConnection::send() 调用发生在该 TcpConnection 所属的那个 IO 线程，
// 那么它会转而调用 TcpConnection::sendInLoop()，sendInLoop() 会在当前线程（也就是 IO 线程）操作 output buffer；
// 情景二: 如果 TcpConnection::send() 调用发生在别的线程，它不会在当前线程调用 sendInLoop() ，
// 而是通过 EventLoop::runInLoop() 把 sendInLoop() 函数调用转移到 IO 线程（听上去颇为神奇？），
// 这样 sendInLoop() 还是会在 IO 线程操作 output buffer，不会有线程安全问题。
// 当然，跨线程的函数转移调用涉及函数参数的跨线程传递，一种简单的做法是把数据拷一份，绝对安全（不明白的同学请阅读代码）。

// 另一种更为高效做法是用 swap()。这就是为什么 TcpConnection::send() 的某个重载以 Buffer* 为参数，而不是 const Buffer&，这样可以避免拷贝，而用 Buffer::swap() 实现高效的线程间数据转移。（最后这点，仅为设想，暂未实现。目前仍然以数据拷贝方式在线程间传递，略微有些性能损失。）

// 本函数的两种情况都是执行sendInLoop
// 但是根据是否跨线程，一种是直接在本线程执行(此时本线程就是loop线程);
// 另一种是将sendInLoop送入loop的任务队列中
void TcpConnection::send(Buffer* buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            LOG_INFO << ( "loop_->isInLoopThread()" );
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        }
        else
        {
            LOG_INFO << ( "not loop_->isInLoopThread()" );
            loop_->runInLoop(
                        boost::bind(&TcpConnection::sendInLoop,
                                    this,     // FIXME
                                    buf->retrieveAllAsString()));
            //std::forward<string>(message)));
        }
    }
}

void TcpConnection::sendInLoop(const StringPiece& message)
{
    sendInLoop(message.data(), message.size());
}

/*
 * 写入数据
 * 1.如果Channel没有监听可写事件且输出缓冲区为空，说明之前没有出现内核缓冲区满的情况，直接写进内核
 * 2.如果写入内核出错，且出错信息(errno)是EWOULDBLOCK，说明内核缓冲区满，将剩余部分添加到应用层输出缓冲区
 * 3.如果之前输出缓冲区为空，那么就没有监听内核缓冲区(fd)可写事件，开始监听
 */
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    LOG_INFO << "sendInLoop len=" << len;

    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    /* 没有写入tcp缓冲区的字节数 */
    size_t remaining = len;
    bool faultError = false;

    /* 当前TcpConnection状态，TcpConnection有四种状态，kDisconnected表示已经断开连接，不能再写了，直接返回 */
    if (state_ == kDisconnected)
    {
        LOG_WARN << "disconnected, give up writing";
        return;
    }

    // if no thing in output queue, try writing directly
    /* 如果输出缓冲区有数据，就不能尝试发送数据了，否则数据会乱，应该直接写到缓冲区中 */
    // 通道没有关注可写事件并且应用层发送缓冲区没有数据，直接write
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            /* 写入了一些数据 */
            remaining = len - nwrote;
            /*
             * 完全写入tcp缓冲区，且用户有提供写数据的回调函数，等待执行完后调用
             * 因为当前TcpConnection和EventLoop所在同一个线程，
             * 而且此时EventLoop通常处在正在处理激活Channel的过程中（当前函数有可能也是在这个过程）
             * 所以等待这个函数执行完再调用回调函数
             */
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            /* 一点也没写进去
             * 如果错误为EWOULDBLOCK，表明tcp缓冲区已满
             */
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                /* EPIPE表示客户端已经关闭了连接，服务器仍然尝试写入，就会出现EPIPE */
                LOG_SYSERR << "TcpConnection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
                {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    // 没有错误，并且还有未写完的数据（说明内核发送缓冲区满，要将未写完的数据添加到output buffer中）
    if (!faultError && remaining > 0)
    {
        /* 获取写缓冲区数据总量 */
        size_t oldLen = outputBuffer_.readableBytes();

        // 如果超过highWaterMark_（高水位标），回调highWaterMarkCallback_
        if (oldLen + remaining >= highWaterMark_
                && oldLen < highWaterMark_
                && highWaterMarkCallback_)
        {
/*
高水位回调和低水位回调：在发送数据时，如果发送过快会造成数据在本地积累。
muduo解决这个问题的办法是用了高水位回调和低水位回调，分别用函数HighWaterMarkCallback和WriteCompleteCallback代表。
原理为：设置一个发送缓冲区的上限值，如果大于这个上限值，停止接收数据；WriteCompleteCallback函数为发送缓冲区为空时调用，在这个函数重启开启接收数据。*/
            loop_->queueInLoop(boost::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }

        //把没有写完的数据追加到输出缓冲区中，然后开启对可写事件的监听（如果之前没开的话）
        //下次调用send 也会将数据添加到outputBuffer_ 的末尾而不直接write），并关注POLLOUT 事件，当内核发送缓冲区不为满，即发生
        outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);
        if (!channel_->isWriting())//如果对应的Channel没有在监听write事件
        {
            //可写事件，调用TcpConnection::handleWrite()
            channel_->enableWriting(); // 关注POLLOUT事件
        }
    }
}

// 半关闭 连接 应用程序想要关闭 但是可能正处于发送数据的过程中 outbuffer 中有数据还没有发送完
// 不能直接调用close()
void TcpConnection::shutdown()
{
    // FIXME: use compare and swap
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        // FIXME: shared_from_this()?
        loop_->runInLoop(boost::bind(&TcpConnection::shutdownInLoop, this));
    }
}

//此时如果应用层缓冲区数据还没发完，即还在关注POLLOUT事件，那么shutdown() 中只是先设置state_ = kDisconnecting; 而 shutdownInLoop() 中判断 isWriting() 为true, 故不会执行shutdownWrite()，回顾handleWrite() 函数，当应用层缓冲区数据发完，判断状态为kDisconnecting 而且已经disableWriting()，就继续调用
//shutdownInLoop() ，此时就会真正关闭写的这一端。
void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting())
    {
        // we are not writing
        socket_->shutdownWrite();
    }
}

// void TcpConnection::shutdownAndForceCloseAfter(double seconds)
// {
//   // FIXME: use compare and swap
//   if (state_ == kConnected)
//   {
//     setState(kDisconnecting);
//     loop_->runInLoop(boost::bind(&TcpConnection::shutdownAndForceCloseInLoop, this, seconds));
//   }
// }

// void TcpConnection::shutdownAndForceCloseInLoop(double seconds)
// {
//   loop_->assertInLoopThread();
//   if (!channel_->isWriting())
//   {
//     // we are not writing
//     socket_->shutdownWrite();
//   }
//   loop_->runAfter(
//       seconds,
//       makeWeakCallback(shared_from_this(),
//                        &TcpConnection::forceCloseInLoop));
// }

void TcpConnection::forceClose()
{
    // FIXME: use compare and swap
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        setState(kDisconnecting);
        loop_->queueInLoop(boost::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        setState(kDisconnecting);
        // not forceCloseInLoop to avoid race condition
        loop_->runAfter(seconds, makeWeakCallback(shared_from_this(), &TcpConnection::forceClose));
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting)
    {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}

const char* TcpConnection::stateToString() const
{
    switch (state_)
    {
    case kDisconnected:
        return "kDisconnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisconnecting:
        return "kDisconnecting";
    default:
        return "unknown state";
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
    loop_->runInLoop(boost::bind(&TcpConnection::startReadInLoop, this));
}

void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading())
    {
        channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::stopRead()
{
    loop_->runInLoop(boost::bind(&TcpConnection::stopReadInLoop, this));
}

void TcpConnection::stopReadInLoop()
{
    loop_->assertInLoopThread();
    if (reading_ || channel_->isReading())
    {
        channel_->disableReading();
        reading_ = false;
    }
}

//TcpServer::newConnection() 创建完 TcpConnection 对象，设置好回调函数之后调用的，
//主要是调用 Channel::enableReading() 将 TcpConnection 对应的 sockfd 注册读事件，
//然后执行用户指定的 connectionCallback_() ，并将 TcpConnection 状态置为 kConnected。
//这个函数如何被执行呢？ Acceptor 持有的 fd 有可读事件发生，
//即有连接请求，此时Channel::handleEvent() -> Acceptor::handleRead() -> TcpServer::newConnection() -> …… ->TcpConnection::connectEstablished()
//中间省略的部分是线程转移，转移到TcpConnection 所在的 IO 线程执行，TcpServer 和 TcpConnection 可能不在同一线程。
void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();//断言处于loop线程
    assert(state_ == kConnecting);

    setState(kConnected);//将状态设置为已连接

    //之前引用计数为2
    EventLoop* ioLoop = getLoop();
    LOG_INFO << "connectEstablished threadId_=" << ioLoop->IoEventLoopTid();// << ", CurTid=" << CurrentThread::tid();

    //将自身这个TcpConnection对象提升，由于是智能指针，所以不能直接用this
    //shared_from_this()之后引用计数+1，为3，但是shared_from_this()是临时对象，析构后又会减一，
    //而tie是weak_ptr并不会改变引用计数，所以该函数执行完之后引用计数不会更改
    channel_->tie(shared_from_this());
    // 对于新建连接的socket描述符，还需要设置期望监控的事件（POLLIN | POLLPRI），
    // 并且将此socket描述符放入poll函数的监控描述符集合中，用于等待接收客户端从此连接上发送来的消息
    // 这些工作，都是由TcpConnection::connectEstablished函数完成。
    channel_->enableReading();

    // 此函数对应TcpServer::connectionCallback_,最终指向一个用户服务器定义的回调函数
    connectionCallback_(shared_from_this());
}

//TcpConnection::connectDestroyed()，这是 TcpConnection 析构前最后调用的一个成员函数，
//它通知用户连接已断开。
//主要功能是设置 TcpConnection 的状态为 kDisconnected；
//停止监听所有事件；
//调用 connectionCallback_() 执行用户指定的回调；
//从 epoll 监听的 fd 中移除 TcpConnection 对应的 sockfd。
//那什么时候调用到这个函数呢？一个是 TcpServer 析构时，一般情况下是经由 TcpConnection::handleClose() -> TcpConnection::closeCallback_() -> TcpServer::removeConnection() -> ……->TcpConnection::connectDestroyed()
//TcpConnection::connectDestroyed是该对象析构前调用的最后一个成员函数，它会通知用户连接已经断开。
void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());// 回调用户设定的回调函数
    }
    channel_->remove();// 从poll/epoll中移除channel
}

/*
muduo采用Level Trigger，而不是Edge Trigger。原因作者提到了三点
1. 在文件描述符较少而且活动文件描述符较多时，ET模式不一定比LT高效；
2. LT编程更容易
3. 读写操作时不必使用循环等候出现EAGAIN，这样可以节省系统调用次数，降低延迟。
此外，作者也提到了理想的方式是读操作使用LT、写操作使用ET，但是目前linux不支持。>_<
*/
/*
 * 1.TcpConnection构造时，创建一个监听服务器/客户端连接的fd的Channel，设置各种回调函数
 * 2.TcpServer设置各种回调函数（可读等），然后调用connectEstablished，将Channel添加到Poller中
 * 3.EventLoop继续监听事件，调用Poller
 * 4.poll返回，处理激活的Channel，调用Channel的handleEvent
 * 5.hanleEvent根据激活事件的类型（可读/可写/挂起/错误）调用不同的处理函数
 * 6.若可读，调用hanleRead，TcpConnection中的读缓冲区将内核tcp缓冲区的数据全部读出
 * 7.调用用户提供的当可读时执行的回调函数，用户可直接从应用层缓冲区读数据
 */
void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        /* 如果成功读取数据，调用用户提供的可读时回调函数 */
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) {
        /* 如果返回0，说明对端已经close连接，处理close事件，关闭tcp连接 */
        handleClose();
    }
    else {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead";
        handleError();
    }
}

//内核发送缓冲区有空间了，回调该函数
//send 一系列函数是可以用户或者其他线程调用，用于发送信息。
//如果不是在IO线程，它会把实际工作转移到IO线程调用。
//首先检查 TcpConnection 对应的 Socket 是否注册了可写事件，若注册了可写事件表明输出缓冲区 outputBuffer_中已经有数据等待发送，
//为了保证不乱序，这次的数据只要追加到输出缓冲区中，通过 Channel::handleEvent() -> TcpConnection::handleWrite() 来发送。
//如果Socket没有注册可写事件，输出缓冲区没有数据，那么这次的消息可以直接通过 write 发送，
//如果没有一次性发送完毕，那么 message 剩余的数据仍然要 append 到 outputBuffer 中，并向 Poller 注册可写事件，
//当 socket 变得可写时，Channel 会调用 TcpConnection::handleWrite() 来发送 outputBuffer_ 中堆积的数据，发送完毕后立刻停止监听可写事件，避免 busy loop。无论是 sendInLoop() -> write() 还是 Channel::handleEvent() -> handleWrite()，只要确定发送完 message 或者 outputBuffer_ 中的数据，那么都要调用用户指定的回调 writeCompleteCallback() 。
void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        /* 尝试写入写缓冲区的所有数据，返回实际写入的字节数（tcp缓冲区很有可能仍然不能容纳所有数据） */
        ssize_t n = sockets::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
//        LOG_INFO << "handleWrite writelen=" << n;
        if (n > 0)
        {
            outputBuffer_.retrieve(n);//调整写缓冲区的readerIndex
            if (outputBuffer_.readableBytes() == 0)// 应用层发送缓冲区已清空
            {
                channel_->disableWriting();// 停止关注POLLOUT事件，以免出现busy loop
                if (writeCompleteCallback_)// 应用层发送缓冲区被清空，就回调用writeCompleteCallback_
                {
                    loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
                }
                /*
                 * 如果连接正在关闭（通常关闭读端），那么关闭写端，但是是在已经写完的前提下
                 * 如果还有数据没有写完，不能关闭，要在写完再关
                 */
                if (state_ == kDisconnecting)
                {
                    /*
                     * 这里的细节问题就是如果想要关闭连接，那么通常是先关闭读端，等到将写缓冲区所有数据都写到tcp缓冲区后，再关闭写端，否则这些数据
                     * 就不能发送给对端了 muduo没有提供close函数，关闭是分两步进行的（使用shutdown而不适用close），这样更容易控制
                     * handleWrite函数中调用的shutdownInLoop函数如下，用于关闭写端
                     * */
                    shutdownInLoop();
                }
                //至此发送数据的操作完成，所以数据都在tcp缓冲区中等待着或正在运往对端（客户端）
            }
        }
        else
        {
            LOG_SYSERR << "TcpConnection::handleWrite";
            // if (state_ == kDisconnecting)
            // {
            //   shutdownInLoop();
            // }
        }
    }
    else
    {
        LOG_TRACE << "Connection fd = " << channel_->fd()
                  << " is down, no more writing";
    }
}

/*
 * TcpConnection的断开是采用被动方式，即对方先关闭连接，本地read(2)返回0后，调用顺序如下：
handleClose()->TcpServer::removeConnection->TcpConnection::connectDestroyed()。
 * */
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();

    LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
    assert(state_ == kConnected || state_ == kDisconnecting);

    // we don't close fd, leave it to dtor, so we can find leaks easily.
    setState(kDisconnected);

    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);
    // must be the last line
    closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    LOG_ERROR << "TcpConnection::handleError [" << name_
              << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

