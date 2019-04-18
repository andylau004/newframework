// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
/*
 * TcpConnection对象不能由removeConnection销毁，因为如果此时Channel中的handleEvent()还在执行的话，会造成core dump，我们使用shared_ptr管理引用计数为1，在Channel中维护一个weak_ptr(tie_)，将这个shared_ptr对象赋值给tie_，引用技术仍为1；当连接关闭，在handleEvent中将tie_提升，得到一个shared_ptr对象，引用计数就为2
 *
 * 一个服务器（TcpServer）维护了一个连接列表，当一个连接断开时，TcpConnection中的通道处于活跃的状态，EventLoop的事件循环返回了这个活跃的通道，然后调用通道的handleEvent()函数来处理。连接关闭是可读事件，进而回调了TcpConnection的handleRead()函数，handleRead()中又调用了read()返回为0，判断read()返回为0又会调用handleClose()函数。handleClose()函数会回调TcpServer的removeConnection()函数，其中会调用erase()将该连接从连接列表移除。

这里我们需要注意的是——一般情况下，将连接从连接列表移除后，我们就可以将这个连接对象销毁（delete）掉了，但是在这里我们不能立即销毁这个连接对象，原因如下：

如果我们销毁了这个对象，TcpConnection所包含的Channel对象也就跟着被销毁了，而我们当前正在调用Channel对象的handleEvent()函数，就会出现core dump。所以，我们必须保证TcpConnection的生存期长于Channel::handleEvent()函数。

muduo选择用智能指针shared_ptr来管理TcpConnection的生命期，并且让TcpConnection类继承自boost::enable_shared_from_this。
 *
 * */
class TcpConnection : boost::noncopyable,
                      public boost::enable_shared_from_this<TcpConnection>
{
public:
    /// Constructs a TcpConnection with a connected sockfd
    ///
    /// User should not create this object.
    TcpConnection(EventLoop* loop,
                  const string& name,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }
    // return true if success.
    bool getTcpInfo(struct tcp_info*) const;
    string getTcpInfoString() const;

    // void send(string&& message); // C++11
    void send(const void* message, int len);
    void send(const StringPiece& message);
    // void send(Buffer&& message); // C++11
    void send(Buffer* message);  // this one will swap data
    void shutdown(); // NOT thread safe, no simultaneous calling
    // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
    void forceClose();
    void forceCloseWithDelay(double seconds);
    void setTcpNoDelay(bool on);
    // reading or not
    void startRead();
    void stopRead();
    bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

    void setContext(const boost::any& context)
    { context_ = context; }

    const boost::any& getContext() const
    { return context_; }

    boost::any* getMutableContext()
    { return &context_; }

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    /// Advanced interface
    Buffer* inputBuffer()
    { return &inputBuffer_; }

    Buffer* outputBuffer()
    { return &outputBuffer_; }

    /// Internal use only.
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }

    // called when TcpServer accepts a new connection
    void connectEstablished();   // should be called only once
    // called when TcpServer has removed me from its map
    void connectDestroyed();  // should be called only once

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    // void sendInLoop(string&& message);
    void sendInLoop(const StringPiece& message);
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    // void shutdownAndForceCloseInLoop(double seconds);
    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; }
    const char* stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop* loop_;
    const string name_;
    StateE state_;  // FIXME: use atomic variable
    bool reading_;
    // we don't expose those classes to client.
    boost::scoped_ptr<Socket> socket_;
    boost::scoped_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;

    // 数据发送完毕的回调函数 即所有的用户数据都拷贝到内核缓冲区回调该函数
    // 如果对等方接受不及时 受到通告窗口的控制 内核发送缓存不足 这个时候 就会将用户数据添加到应用层发送缓冲区outbuffer
    // 可能会撑爆outbuffer 解决方法:调整发送频率 关注writeCompleCallback

    // 将所有的数据都发送完 writeCompleCallback回调 继续发送
    WriteCompleteCallback writeCompleteCallback_;// 低水位回调函数
    HighWaterMarkCallback highWaterMarkCallback_;// 高水位回调函数 outbuffer快满了

    CloseCallback closeCallback_;
    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.
    boost::any context_;
    // FIXME: creationTime_, lastReceiveTime_
    //        bytesReceived_, bytesSent_
};

typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
