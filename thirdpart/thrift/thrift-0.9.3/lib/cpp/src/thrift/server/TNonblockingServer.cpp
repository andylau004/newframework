/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#define __STDC_FORMAT_MACROS

#include <thrift/thrift-config.h>

#include <thrift/server/TNonblockingServer.h>
#include <thrift/concurrency/Exception.h>
#include <thrift/transport/TSocket.h>
#include <thrift/concurrency/PlatformThreadFactory.h>
#include <thrift/transport/PlatformSocket.h>

#include <iostream>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef __linux__
#include <poll.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <assert.h>

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

#if !defined(PRIu32)
#define PRIu32 "I32u"
#define PRIu64 "I64u"
#endif


#include "../../../../../../../../muduo/base/common.h"
#include "../../../../../../../../muduo/base/CurrentThread.h"
#include "../../../../../../../../muduo/base/Logging.h"

#include "../../../../../../../../muduo/base/event_watcher.h"



namespace apache {
namespace thrift {
namespace server {

using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::concurrency;
using namespace std;
using apache::thrift::transport::TSocket;
using apache::thrift::transport::TTransportException;
using boost::shared_ptr;


// socket状态转移
// 因为是异步编程，每个socket都必须设置为非阻塞。
// 当可读的事件发生时，则读，可写的事件发生时，则写。
// 读和写两种操作会互相交替进行，所以我们需要用 状态值来进行不同的逻辑处理。
// TNonblockingServer 里的状态值有以下三种：
/// Three states for sockets: recv frame size, recv data, and send mode
enum TSocketState { SOCKET_RECV_FRAMING, SOCKET_RECV, SOCKET_SEND };

// 需要补充说明的是，要和 Thrift 的 TNonblockingServer 通信，则客户端 需要使用
// shared_ptr<TTransport> transport(new TFramedTransport(socket));

// 来作为传输工具，就是因为 TNonblockingServer 的 socket recv 数据是 按 frame 来一帧帧的接受。
// 所以第一个状态值 SOCKET_RECV_FRAMING 代表进入该状态就是有帧头（数据包的大小）可以读取， 
// 而第二个状态值 SOCKET_RECV 代表有数据可以读取，先读完帧头才读该数据。 第三个状态 SOCKET_SEND 代表有数据可以发送。

// 每次 rpc 调用的过程的状态转移先后过程如下：
// SOCKET_RECV_FRAMING -> SOCKET_RECV -> SOCKET_SEND
// 这三个状态都有可能被重复调用，取决于数据包的大小。

// 每次 socket 状态转移靠 workSocket 函数完成

/**
 * Five states for the nonblocking server:
 *  1) initialize
 *  2) read 4 byte frame size
 *  3) read frame of data
 *  4) send back data (if any)
 *  5) force immediate connection close
 */
// app状态转移
// 上面的 socket 状态转移，是针对每个连接的数据收发状态转移， 
// 和 socket 紧密相关，而这里的 app 状态转移则是针对整个 rpc 远程函数调用
// (不过每次rpc调用其实也是建立在某个连接的基础之上)。
// app状态的代码如下：
enum TAppState {
    APP_INIT,
    APP_READ_FRAME_SIZE,
    APP_READ_REQUEST,
    APP_WAIT_TASK,
    APP_SEND_RESULT,
    APP_CLOSE_CONNECTION
};

// 状态的转移顺序如下：
// 1. APP_INIT 初始状态。
// 2. APP_READ_FRAME_SIZE 读取帧数据。
// 3. APP_READ_REQUEST 读取请求的数据，并根据请求的数据 进行数据的解析和任务的生成，并且将任务扔进线程池。
// 4. APP_WAIT_TASK 等待任务的完成
// 5. APP_SEND_RESULT 任务已经完成，将任务结果发送。
// 6. APP_CLOSE_CONNECTION 关闭连接。

// 每次app状态转移由 TConnetion::transition 函数完成：
// void transition();
// 状态3 -> 状态4 -> 状态5 转移很关键，涉及到线程池和主线程的交互。

/**
 * Represents a connection that is handled via libevent. This connection
 * essentially encapsulates a socket that has some associated libevent state.
 *
 * 在TNonBlockingServer中负责处理连接管理，即数据的输入和输出。每个TConnection和一个TNonBlockingIOThread关联，通过其可以获得所在Server的句柄。（但是一个TNonBlockingIOThread可能被多个TConnection关联。）同时记录一个套接字连接的基本信息（sockaddr）。套接字信息存放在内部的TSocket类型的属性中。其他重要属性如下：

a. 输入输出分别为inputTransport和outputTransport属性，他们两个都是TMemoryBuffer的对象指针，实质上就是输入缓冲和输出缓冲。factoryInputTransport_和factoryOutputTransport_是对输入缓冲和输出缓冲的封装。这里，应该是封装为TFrameTransport。

b. 在序列化协议方面，inputProtocol_和outputProtocol_两个属性分别是输入和输出的序列化协议。一般使用TBinaryProtocol。不过，也支持自定义的特殊Transport（没有继承TTransport）的上面架设二进制序列化协议，只是需要传入特定的类型给模板。在这里没有传入类型，可见TNonblockingServer只支持继承TTransport的传输协议，实际上，只支持TFrameTransport。

c. connectionContext。如果创建Server的时候指定了serverEventHandler，那么这里新建connectionContext。实现是调用handler的createContext方法。

d. processor。这个调用比较诡异，他传入了三个参数：inputProtocol_、outputProtocol_还有tSocket_，但是下层调用的时候根本就没有使用这三个参数，而是直接调用的TProcessorFactory的getProcessor。这个方法是个纯虚方法，考察得到该类只有一个派生类，TSingletonProcessorFactory，这里的方法调用时候，根本就没有使用到之前传递的三个参数，而是直接返回的processor_。这是一个单例，可以推测，一个TSingletonProcessorFactory只会拥有一个processor，而且注释要求，所有调用这个方法的线程不得同时出现两个。

构造方法会调用init方法，最终完成所有属性的初始化。除了上面这些属性，比较重要的初始化还有两个，appState<--APP_INIT，socketState<--SOCKET_RECV_FRAMING。这两个State主要是用于TConnection以状态机的方式进行工作预设。
 */
class TNonblockingServer::TConnection {
private:
    /// Server IO Thread handling this connection
    TNonblockingIOThread* ioThread_;

    /// Server handle
    TNonblockingServer* server_;

    /// TProcessor
    /// d. processor。这个调用比较诡异，他传入了三个参数：inputProtocol_、outputProtocol_还有tSocket_，但是下层调用的时候根本就没有使用这三个参数，而是直接调用的TProcessorFactory的getProcessor。这个方法是个纯虚方法，考察得到该类只有一个派生类，TSingletonProcessorFactory，这里的方法调用时候，根本就没有使用到之前传递的三个参数，而是直接返回的processor_。这是一个单例，可以推测，一个TSingletonProcessorFactory只会拥有一个processor，而且注释要求，所有调用这个方法的线程不得同时出现两个。
    boost::shared_ptr<TProcessor> processor_;

    /// Object wrapping network socket
    boost::shared_ptr<TSocket> tSocket_;

    /// Libevent object
    struct event event_;

    /// Libevent flags
    short eventFlags_;

    /// Socket mode
    TSocketState socketState_;

    /// Application state
    TAppState appState_;

    /// How much data needed to read
    uint32_t readWant_;

    /// Where in the read buffer are we
    uint32_t readBufferPos_;

    /// Read buffer
    uint8_t* readBuffer_;

    /// Read buffer size
    uint32_t readBufferSize_;

    /// Write buffer
    uint8_t* writeBuffer_;

    /// Write buffer size
    uint32_t writeBufferSize_;

    /// How far through writing are we?
    uint32_t writeBufferPos_;

    /// Largest size of write buffer seen since buffer was constructed
    size_t largestWriteBufferSize_;

    /// Count of the number of calls for use with getResizeBufferEveryN().
    int32_t callsForResize_;


    /// a. 输入输出分别为inputTransport和outputTransport属性，他们两个都是TMemoryBuffer的对象指针，实质上就是输入缓冲和输出缓冲。factoryInputTransport_和factoryOutputTransport_是对输入缓冲和输出缓冲的封装。这里，应该是封装为TFrameTransport。
    /// Transport to read from
    boost::shared_ptr<TMemoryBuffer> inputTransport_;

    /// Transport that processor writes to
    boost::shared_ptr<TMemoryBuffer> outputTransport_;

    /// extra transport generated by transport factory (e.g. BufferedRouterTransport)
    boost::shared_ptr<TTransport> factoryInputTransport_;
    boost::shared_ptr<TTransport> factoryOutputTransport_;


    /// b. 在序列化协议方面，inputProtocol_和outputProtocol_两个属性分别是输入和输出的序列化协议。一般使用TBinaryProtocol。不过，也支持自定义的特殊Transport（没有继承TTransport）的上面架设二进制序列化协议，只是需要传入特定的类型给模板。在这里没有传入类型，可见TNonblockingServer只支持继承TTransport的传输协议，实际上，只支持TFrameTransport。
    /// Protocol decoder
    boost::shared_ptr<TProtocol> inputProtocol_;

    /// Protocol encoder
    boost::shared_ptr<TProtocol> outputProtocol_;

    /// c. connectionContext。如果创建Server的时候指定了serverEventHandler，那么这里新建connectionContext。实现是调用handler的createContext方法。
    /// Server event handler, if any
    boost::shared_ptr<TServerEventHandler> serverEventHandler_;

    /// Thrift call context, if any
    void* connectionContext_;

    /// Go into read mode
    void setRead() { setFlags(EV_READ | EV_PERSIST); }

    /// Go into write mode
    void setWrite() { setFlags(EV_WRITE | EV_PERSIST); }

    /// Set socket idle
    void setIdle() { setFlags(0); }

    /**
   * Set event flags for this connection.
   *
   * @param eventFlags flags we pass to libevent for the connection.
   */
    void setFlags(short eventFlags);

    /**
   * Libevent handler called (via our static wrapper) when the connection
   * socket had something happen.  Rather than use the flags libevent passed,
   * we use the connection state to determine whether we need to read or
   * write the socket.
   */
    void workSocket();

public:
    class Task;

    /// Constructor
    TConnection(THRIFT_SOCKET socket,
                TNonblockingIOThread* ioThread,
                const sockaddr* addr,
                socklen_t addrLen) {
        readBuffer_ = NULL;
        readBufferSize_ = 0;

        ioThread_ = ioThread;
        server_ = ioThread->getServer();

        // Allocate input and output transports these only need to be allocated
        // once per TConnection (they don't need to be reallocated on init() call)
        inputTransport_.reset(new TMemoryBuffer(readBuffer_, readBufferSize_));
        outputTransport_.reset(new TMemoryBuffer(static_cast<uint32_t>(server_->getWriteBufferDefaultSize())));
        tSocket_.reset(new TSocket());
        init(socket, ioThread, addr, addrLen);
    }

    ~TConnection() { std::free(readBuffer_); }

    /// Close this connection and free or reset its resources.
    void close();

    /**
    * Check buffers against any size limits and shrink it if exceeded.
    *
    * @param readLimit we reduce read buffer size to this (if nonzero).
    * @param writeLimit if nonzero and write buffer is larger, replace it.
    */
    void checkIdleBufferMemLimit(size_t readLimit, size_t writeLimit);

    /// Initialize
    void init(THRIFT_SOCKET socket,
              TNonblockingIOThread* ioThread,
              const sockaddr* addr,
              socklen_t addrLen);

    /**
   * This is called when the application transitions from one state into
   * another. This means that it has finished writing the data that it needed
   * to, or finished receiving the data that it needed to.
   */
    void transition();

    /**
   * C-callable event handler for connection events.  Provides a callback
   * that libevent can understand which invokes connection_->workSocket().
   *
   * @param fd the descriptor the event occurred on.
   * @param which the flags associated with the event.
   * @param v void* callback arg where we placed TConnection's "this".
   */
    static void eventHandler(evutil_socket_t fd, short /* which */, void* v) {
        assert(fd == static_cast<evutil_socket_t>(((TConnection*)v)->getTSocket()->getSocketFD()));
        ((TConnection*)v)->workSocket();
    }

    /**
   * Notification to server that processing has ended on this request.
   * Can be called either when processing is completed or when a waiting
   * task has been preemptively terminated (on overload).
   *
   * Don't call this from the IO thread itself.
   *
   * @return true if successful, false if unable to notify (check THRIFT_GET_SOCKET_ERROR).
   */
    bool notifyIOThread() { return ioThread_->notify(this); }

    /*
   * Returns the number of this connection's currently assigned IO
   * thread.
   */
    int getIOThreadNumber() const { return ioThread_->getThreadNumber(); }

    /// Force connection shutdown for this connection.
    void forceClose() {
        appState_ = APP_CLOSE_CONNECTION;
        if (!notifyIOThread()) {
            // new add fy
            server_->decrementActiveProcessors();
            close();
            throw TException("TConnection::forceClose: failed write on notify pipe");
        }
    }

    /// return the server this connection was initialized for.
    TNonblockingServer* getServer() const { return server_; }

    /// get state of connection.
    TAppState getState() const { return appState_; }

    /// return the TSocket transport wrapping this network connection
    boost::shared_ptr<TSocket> getTSocket() const { return tSocket_; }

    /// return the server event handler if any
    boost::shared_ptr<TServerEventHandler> getServerEventHandler() { return serverEventHandler_; }

    /// return the Thrift connection context if any
    void* getConnectionContext() { return connectionContext_; }
};

class TNonblockingServer::TConnection::Task : public Runnable {
public:
    Task(boost::shared_ptr<TProcessor> processor,
         boost::shared_ptr<TProtocol> input,
         boost::shared_ptr<TProtocol> output,
         TConnection* connection)
        : processor_(processor),
          input_(input),
          output_(output),
          connection_(connection),
          serverEventHandler_(connection_->getServerEventHandler()),
          connectionContext_(connection_->getConnectionContext()) {}

    void run() {
        try {
            for (;;) {
                //                std::cout << __FILE__ << ":" << __LINE__ << "    TNonblockingServer::TConnection::Task serverEventHandler_=" << serverEventHandler_ << std::endl;
                GlobalOutput.printf( "workthread TNonblockingServer::TConnection::Task serverEventHandler_=%p", serverEventHandler_.get() );
                if (serverEventHandler_) {
                    serverEventHandler_->processContext(connectionContext_, connection_->getTSocket());
                }
                // 回调
                if (!processor_->process(input_, output_, connectionContext_) ||
                    !input_->getTransport()->peek()) {
                    break;
                }
            }
        } catch (const TTransportException& ttx) {
            GlobalOutput.printf("TNonblockingServer: client died: %s", ttx.what());
        } catch (const bad_alloc&) {
            GlobalOutput("TNonblockingServer: caught bad_alloc exception.");
            exit(1);
        } catch (const std::exception& x) {
            GlobalOutput.printf("TNonblockingServer: process() exception: %s: %s",
                                typeid(x).name(), x.what());
        } catch (...) {
            GlobalOutput.printf("TNonblockingServer: unknown exception while processing.");
        }

        // Signal completion back to the libevent thread via a pipe
        // 而当线程池里的某个 Task 运行完毕后，
        // 则会触发主线程的 pipe_event (上文中的已注册事件种的第二种事件)，告知主线程任务已完成。

        // 回调完后通知，
        // 从工作线程重回到IO线程
        // connection_的指针地址将通过管道传给工作线程
        // workpool thread 任务线程处理完成后会调用notifyIOThread通知connection对应的iothead来发送结果给客户端
        if (!connection_->notifyIOThread()) {
            connection_->server_->decrementActiveProcessors();
            GlobalOutput.printf("TNonblockingServer: failed to notifyIOThread, closing.");
            connection_->close();
            throw TException("TNonblockingServer::Task::run: failed write on notify pipe");
        }
        // 主线程收到通知之后，则会从 状态4(APP_WAIT_TASK) 转 移向 状态5(APP_SEND_RESULT)，
        // 进入向 客户端发送函数调用结果的过程。
    }

    TConnection* getTConnection() { return connection_; }

private:
    boost::shared_ptr<TProcessor> processor_;
    boost::shared_ptr<TProtocol> input_;
    boost::shared_ptr<TProtocol> output_;
    TConnection* connection_;
    boost::shared_ptr<TServerEventHandler> serverEventHandler_;
    void* connectionContext_;
};

void TNonblockingServer::TConnection::init(THRIFT_SOCKET socket,
                                           TNonblockingIOThread* ioThread,
                                           const sockaddr* addr,
                                           socklen_t addrLen) {
    tSocket_->setSocketFD(socket);
    tSocket_->setCachedAddress(addr, addrLen);

    ioThread_ = ioThread;
    server_ = ioThread->getServer();
    appState_ = APP_INIT;
    eventFlags_ = 0;

    readBufferPos_ = 0;
    readWant_ = 0;

    writeBuffer_ = NULL;
    writeBufferSize_ = 0;
    writeBufferPos_ = 0;
    largestWriteBufferSize_ = 0;

    socketState_ = SOCKET_RECV_FRAMING;
    callsForResize_ = 0;

    GlobalOutput.printf( "inputTransport_=%p outputTransport_=%p\n",
                         inputTransport_.get(), outputTransport_.get() );

    // get input/transports
    factoryInputTransport_  = server_->getInputTransportFactory()->getTransport(inputTransport_);
    factoryOutputTransport_ = server_->getOutputTransportFactory()->getTransport(outputTransport_);

    // Create protocol
    inputProtocol_  = server_->getInputProtocolFactory()->getProtocol(factoryInputTransport_);
    outputProtocol_ = server_->getOutputProtocolFactory()->getProtocol(factoryOutputTransport_);

    // Set up for any server event handler
    serverEventHandler_ = server_->getEventHandler();
    if (serverEventHandler_) {
        GlobalOutput.printf( "serverEventHandler_ != null" );
        connectionContext_ = serverEventHandler_->createContext(inputProtocol_, outputProtocol_);
    } else {
        GlobalOutput.printf( "serverEventHandler_ == null" );
        connectionContext_ = nullptr;
    }

    // Get the processor
    processor_ = server_->getProcessor(inputProtocol_, outputProtocol_, tSocket_);
    GlobalOutput.printf( "processor_=%p inputProtocol_=%p outputProtocol_=%p tSocket_=%p\n",
                         processor_.get(), inputProtocol_.get(), outputProtocol_.get(), tSocket_.get() );
}

void TNonblockingServer::TConnection::workSocket() {
    int got = 0, left = 0, sent = 0;
    uint32_t fetch = 0;

    switch (socketState_) {
    case SOCKET_RECV_FRAMING:
        union {
            uint8_t buf[sizeof(uint32_t)];
            uint32_t size;
        } framing;

        // if we've already received some bytes we kept them here
        framing.size = readWant_;

        try {// determine size of this frame
        fetch = tSocket_->read(&framing.buf[readBufferPos_],
                               uint32_t(sizeof(framing.size) - readBufferPos_));
        if (fetch == 0) {
            // Whenever we get here it means a remote disconnect
            close();
            return;
        }
        readBufferPos_ += fetch;
    } catch (TTransportException& te) {
            GlobalOutput.printf("TConnection::workSocket(): %s", te.what());
            close();
            return;
        }

        if (readBufferPos_ < sizeof(framing.size)) {
            // more needed before frame size is known -- save what we have so far
            readWant_ = framing.size;
            return;
        }

        readWant_ = ntohl(framing.size);
        if (readWant_ > server_->getMaxFrameSize()) {
            // Don't allow giant frame sizes.  This prevents bad clients from
            // causing us to try and allocate a giant buffer.
            GlobalOutput.printf(
                        "TNonblockingServer: frame size too large "
                        "(%" PRIu32 " > %" PRIu64
                        ") from client %s. "
                        "Remote side not using TFramedTransport?",
                        readWant_,
                        (uint64_t)server_->getMaxFrameSize(),
                        tSocket_->getSocketInfo().c_str());
            close();
            return;
        }
        // size known; now get the rest of the frame
        transition();
        return;

    case SOCKET_RECV:
        // It is an error to be in this state if we already have all the data
        assert(readBufferPos_ < readWant_);

        try {
        // Read from the socket
        fetch = readWant_ - readBufferPos_;
        got = tSocket_->read(readBuffer_ + readBufferPos_, fetch);
    } catch (TTransportException& te) {
            GlobalOutput.printf("TConnection::workSocket(): %s", te.what());
            close();
            return;
        }

        if (got > 0) {
            // Move along in the buffer
            readBufferPos_ += got;

            // Check that we did not overdo it
            assert(readBufferPos_ <= readWant_);

            // We are done reading, move onto the next state
            if (readBufferPos_ == readWant_) {
                transition();
            }
            return;
        }

        // Whenever we get down here it means a remote disconnect
        close();
        return;

    case SOCKET_SEND:
        // Should never have position past size
        assert(writeBufferPos_ <= writeBufferSize_);

        // If there is no data to send, then let us move on
        if (writeBufferPos_ == writeBufferSize_) {
            GlobalOutput("WARNING: Send state with no data to send\n");
            transition();
            return;
        }

        try {
        left = writeBufferSize_ - writeBufferPos_;
        sent = tSocket_->write_partial(writeBuffer_ + writeBufferPos_, left);
    } catch (TTransportException& te) {
            GlobalOutput.printf("TConnection::workSocket(): %s ", te.what());
            close();
            return;
        }

        writeBufferPos_ += sent;

        // Did we overdo it?
        assert(writeBufferPos_ <= writeBufferSize_);

        // We are done!
        if (writeBufferPos_ == writeBufferSize_) {
            transition();
        }
        return;

    default:
        GlobalOutput.printf("Unexpected Socket State %d", socketState_);
        assert(0);
    }
}

int  g_work_step = 1;

/**
 * This is called when the application transitions from one state into
 * another. This means that it has finished writing the data that it needed
 * to, or finished receiving the data that it needed to.
 */
// transition()为状态迁移函数
void TNonblockingServer::TConnection::transition() {
    // ensure this connection is active right now
    assert(ioThread_);
    assert(server_);

    // Switch upon the state that we are currently in and move to a new state
    switch (appState_) {

    // APP_READ_REQUEST 状态发生在IO线程中，addTask()会将任务转交给或工作线程，然后由工作线程回调服务端的函数。
    // 当 TConnetion 的 app状态 进入 APP_READ_REQUEST 之后
    // 读取完请求数据之后，则将任务包装好扔进线程池。 并且将状态改变(APP_READ_REQUEST -> APP_WAIT_TASK)：
    // 当connection的一个请求数据read完成时, 封装成任务task交由workpool thread 的线程池处理
    // 3 --
    case APP_READ_REQUEST:
        //        GlobalOutput.printf("TConnection::transition workflow APP_READ_REQUEST");
        GlobalOutput.printf("workflow ----------- %d APP_READ_REQUEST", g_work_step++);

        // We are done reading the request, package the read buffer into transport
        // and get back some data from the dispatch function
        inputTransport_->resetBuffer(readBuffer_, readBufferPos_);
        outputTransport_->resetBuffer();

        // Prepend four bytes of blank space to the buffer so we can
        // write the frame size there later.
        outputTransport_->getWritePtr(4);
        outputTransport_->wroteBytes(4);

        server_->incrementActiveProcessors();

        if (server_->isThreadPoolProcessing()) {// 是否是工作线程池模式
            // We are setting up a Task to do this work and we will wait on it
            //            std::cout << __FILE__ << ":" << __LINE__ << "    server_->isThreadPoolProcessing == true" << std::endl;
            //            printf( "                                 tid=%d  server_->isThreadPoolProcessing == true\n", CurTid() );
            //            GlobalOutput.printf("server_->isThreadPoolProcessing == true");
            GlobalOutput.printf("workflow -----------   server_->isThreadPoolProcessing == true");

            // Create task and dispatch to the thread manager
            boost::shared_ptr<Runnable> task
                    = boost::shared_ptr<Runnable>(new Task(processor_, inputProtocol_, outputProtocol_, this));

            // The application is now waiting on the task to finish
            appState_ = APP_WAIT_TASK;

            // Set this connection idle so that libevent doesn't process more
            // data on it while we're still waiting for the threadmanager to
            // finish this task
            // setIdle目的在于将该连接对应的socket事件标志位清空，
            // 也就是在Idle阶段不再关心该socket是否有数据可读或者可写。
            setIdle();// 最新版本thrift把设置空闲调整到此处

            try {// server_为TNonblockingServer回调交给工作线程，IO线程不做这个工作
                server_->addTask(task);
            } catch (IllegalStateException& ise) {
                server_->decrementActiveProcessors();
                // The ThreadManager is not ready to handle any more tasks (it's probably shutting down).
                GlobalOutput.printf("IllegalStateException: Server::process() %s", ise.what());
                close();
            } catch (TimedOutException& to) {
                server_->decrementActiveProcessors();
                GlobalOutput.printf("[ERROR] TimedOutException: Server::process() %s", to.what());
                close();
            }
            return;
        } else {
            // 调用TNonblockingServer的构造函数时，
            // 如果没有指定参数ThreadManager，则会走这条分支
            // 这种情况下，isThreadPoolProcessing()返回false
            std::cout << __FILE__ << ":" << __LINE__ << "    server_->isThreadPoolProcessing == false" << std::endl;
            std::cout << __FILE__ << ":" << __LINE__ << "    serverEventHandler_ = " << serverEventHandler_ << std::endl;

            try {
                if (serverEventHandler_) {
                    serverEventHandler_->processContext(connectionContext_, getTSocket());
                }
                // Invoke the processor
                processor_->process(inputProtocol_, outputProtocol_, connectionContext_);
            } catch (const TTransportException& ttx) {
                GlobalOutput.printf(
                            "TNonblockingServer transport error in "
                            "process(): %s",
                            ttx.what());
                server_->decrementActiveProcessors();
                close();
                return;
            } catch (const std::exception& x) {
                GlobalOutput.printf("Server::process() uncaught exception: %s: %s",
                                    typeid(x).name(),
                                    x.what());
                server_->decrementActiveProcessors();
                close();
                return;
            } catch (...) {
                GlobalOutput.printf("Server::process() unknown exception");
                server_->decrementActiveProcessors();
                close();
                return;
            }
        }

        // Intentionally fall through here, the call to process has written into
        // the writeBuffer_
        // 4 --
    case APP_WAIT_TASK:
        //        GlobalOutput.printf("TConnection::transition workflow APP_WAIT_TASK");
        GlobalOutput.printf("workflow ----------- %d APP_WAIT_TASK", g_work_step++);

        // We have now finished processing a task and the result has been written
        // into the outputTransport_, so we grab its contents and place them into
        // the writeBuffer_ for actual writing by the libevent thread
        server_->decrementActiveProcessors();

        // Get the result of the operation
        outputTransport_->getBuffer(&writeBuffer_, &writeBufferSize_);

        // If the function call generated return data, then move into the send
        // state and get going
        // 4 bytes were reserved for frame size
        if (writeBufferSize_ > 4) {

            // Move into write state
            writeBufferPos_ = 0;
            socketState_    = SOCKET_SEND;

            // Put the frame size into the write buffer
            int32_t frameSize = (int32_t)htonl(writeBufferSize_ - 4);
            memcpy(writeBuffer_, &frameSize, 4);

            // Socket into write mode
            appState_ = APP_SEND_RESULT;
            setWrite();

            // Try to work the socket immediately
            // workSocket();
            return;
        }

        // In this case, the request was oneway and we should fall through
        // right back into the read frame header state
        goto LABEL_APP_INIT;

        // 5 --
    case APP_SEND_RESULT:
        //        GlobalOutput.printf("TConnection::transition workflow APP_SEND_RESULT");
        GlobalOutput.printf("workflow ----------- %d APP_SEND_RESULT", g_work_step++);

        // it's now safe to perform buffer size housekeeping.
        if (writeBufferSize_ > largestWriteBufferSize_) {
            largestWriteBufferSize_ = writeBufferSize_;
        }
        if (server_->getResizeBufferEveryN() > 0 && ++callsForResize_ >= server_->getResizeBufferEveryN()) {
            checkIdleBufferMemLimit(server_->getIdleReadBufferLimit(), server_->getIdleWriteBufferLimit());
            callsForResize_ = 0;
        }

        // N.B.: We also intentionally fall through here into the INIT state!

LABEL_APP_INIT:
    case APP_INIT:// 1 -- 初始状态
        g_work_step = 1;
        GlobalOutput.printf("workflow ----------- %d APP_INIT", g_work_step++);

        // Clear write buffer variables
        writeBuffer_ = NULL;
        writeBufferPos_ = 0;
        writeBufferSize_ = 0;

        // Into read4 state we go
        socketState_ = SOCKET_RECV_FRAMING;
        appState_    = APP_READ_FRAME_SIZE;

        readBufferPos_ = 0;

        // Register read event
        setRead();

        // Try to work the socket right away
        // workSocket();
        return;

    case APP_READ_FRAME_SIZE:// 2 -- 读取4字节数据帧头
        //        GlobalOutput.printf("TConnection::transition workflow APP_READ_FRAME_SIZE");
        GlobalOutput.printf("workflow ----------- %d APP_READ_FRAME_SIZE", g_work_step++);

        // We just read the request length
        // Double the buffer size until it is big enough
        if (readWant_ > readBufferSize_) {
            if (readBufferSize_ == 0) {
                readBufferSize_ = 1;
            }
            uint32_t newSize = readBufferSize_;
            while (readWant_ > newSize) {
                newSize *= 2;
            }

            uint8_t* newBuffer = (uint8_t*)std::realloc(readBuffer_, newSize);
            if (newBuffer == NULL) {
                // nothing else to be done...
                throw std::bad_alloc();
            }
            readBuffer_ = newBuffer;
            readBufferSize_ = newSize;
        }

        readBufferPos_ = 0;

        // Move into read request state
        socketState_ = SOCKET_RECV;
        appState_ = APP_READ_REQUEST;

        // Work the socket right away
        // workSocket();
        return;

    case APP_CLOSE_CONNECTION:
        GlobalOutput.printf("workflow ----------- %d APP_CLOSE_CONNECTION", g_work_step++);
        server_->decrementActiveProcessors();
        close();
        return;

    default:
        GlobalOutput.printf("workflow ----------- %d default", g_work_step++);
        GlobalOutput.printf("Unexpected Application State %d", appState_);
        assert(0);
    }
}

void TNonblockingServer::TConnection::setFlags(short eventFlags) {
    // Catch the do nothing case
    if (eventFlags_ == eventFlags) {
        return;
    }

    // Delete a previously existing event
    if (eventFlags_ != 0) {
        if (event_del(&event_) == -1) {
            GlobalOutput("TConnection::setFlags event_del");
            return;
        }
    }

    // Update in memory structure
    eventFlags_ = eventFlags;

    // Do not call event_set if there are no flags
    if (!eventFlags_) {
        return;
    }

    /*
   * event_set:
   *
   * Prepares the event structure &event to be used in future calls to
   * event_add() and event_del().  The event will be prepared to call the
   * eventHandler using the 'sock' file descriptor to monitor events.
   *
   * The events can be either EV_READ, EV_WRITE, or both, indicating
   * that an application can read or write from the file respectively without
   * blocking.
   *
   * The eventHandler will be called with the file descriptor that triggered
   * the event and the type of event which will be one of: EV_TIMEOUT,
   * EV_SIGNAL, EV_READ, EV_WRITE.
   *
   * The additional flag EV_PERSIST makes an event_add() persistent until
   * event_del() has been called.
   *
   * Once initialized, the &event struct can be used repeatedly with
   * event_add() and event_del() and does not need to be reinitialized unless
   * the eventHandler and/or the argument to it are to be changed.  However,
   * when an ev structure has been added to libevent using event_add() the
   * structure must persist until the event occurs (assuming EV_PERSIST
   * is not set) or is removed using event_del().  You may not reuse the same
   * ev structure for multiple monitored descriptors; each descriptor needs
   * its own ev.
   */
    // 每个连接的状态变化事件，每一个 TConnection 代表一个连接，
    // 每一个 TConnection 含有一个 socket 文件描述符，
    // 并且当 TConnection 生成之后，会为它注册一个事件，负责对该 socket 的异步读写。
    // 注意到，每个连接都会注册一个 第三种事件，
    // 也就是说，程序的整个运行过程中，假设并发连接数为 n，
    // 则第三种事件的数量也为 n，而第一种和第二种始终 只有一个事件。 所以整个程序运行过程中事件的数量是【2 + n】。
    event_set(&event_, tSocket_->getSocketFD(), eventFlags_, TConnection::eventHandler, this);
    event_base_set(ioThread_->getEventBase(), &event_);

    // Add the event
    if (event_add(&event_, 0) == -1) {
        GlobalOutput.perror("TConnection::setFlags(): could not event_add", THRIFT_GET_SOCKET_ERROR);
//        GlobalOutput("TConnection::setFlags(): could not event_add");
    }
}

/**
 * Closes a connection
 */
void TNonblockingServer::TConnection::close() {
    // Delete the registered libevent
    if (event_del(&event_) == -1) {
        GlobalOutput.perror("TConnection::close() event_del", THRIFT_GET_SOCKET_ERROR);
    }

    if (serverEventHandler_) {
        serverEventHandler_->deleteContext(connectionContext_, inputProtocol_, outputProtocol_);
    }
    ioThread_ = NULL;

    // Close the socket
    tSocket_->close();

    // close any factory produced transports
    factoryInputTransport_->close();
    factoryOutputTransport_->close();

    // release processor and handler
    processor_.reset();

    // Give this object back to the server that owns it
    server_->returnConnection(this);
}

void TNonblockingServer::TConnection::checkIdleBufferMemLimit(size_t readLimit, size_t writeLimit) {
    if (readLimit > 0 && readBufferSize_ > readLimit) {
        free(readBuffer_);
        readBuffer_ = NULL;
        readBufferSize_ = 0;
    }

    if (writeLimit > 0 && largestWriteBufferSize_ > writeLimit) {
        // just start over
        outputTransport_->resetBuffer(static_cast<uint32_t>(server_->getWriteBufferDefaultSize()));
        largestWriteBufferSize_ = 0;
    }
}

TNonblockingServer::~TNonblockingServer() {
    // Close any active connections (moves them to the idle connection stack)
    while (activeConnections_.size()) {
        activeConnections_.front()->close();
    }
    // Clean up unused TConnection objects in connectionStack_
    while (!connectionStack_.empty()) {
        TConnection* connection = connectionStack_.top();
        connectionStack_.pop();
        delete connection;
    }
    // The TNonblockingIOThread objects have shared_ptrs to the Thread
    // objects and the Thread objects have shared_ptrs to the TNonblockingIOThread
    // objects (as runnable) so these objects will never deallocate without help.
    while (!ioThreads_.empty()) {
        boost::shared_ptr<TNonblockingIOThread> iot = ioThreads_.back();
        ioThreads_.pop_back();
        iot->setThread(boost::shared_ptr<Thread>());
    }
}

/**
 * Creates a new connection either by reusing an object off the stack or
 * by allocating a new one entirely
 */
// 采用轮询的方式选择线程 TConnection将自己绑定到一个线程
// 使用了内存池connectionStack_
// App状态：APP_INIT
// Socket状态：SOCKET_RECV_FRAMING
TNonblockingServer::TConnection* TNonblockingServer::createConnection(THRIFT_SOCKET socket,
                                                                      const sockaddr* addr,
                                                                      socklen_t addrLen) {
    // Check the stack
    Guard g(connMutex_);

    // pick an IO thread to handle this connection -- currently round robin
    assert(nextIOThread_ < ioThreads_.size());
    uint32_t selectedThreadIdx = nextIOThread_;
    nextIOThread_ = static_cast<uint32_t>((nextIOThread_ + 1) % ioThreads_.size());

    TNonblockingIOThread* ioThread = ioThreads_[selectedThreadIdx].get();
    GlobalOutput.printf( "clientsocket:%d bind iothreadIdx=%d nextIOThread_=%d iothreadsize=%d",
                         socket, selectedThreadIdx, nextIOThread_, ioThreads_.size() );

    // Check the connection stack to see if we can re-use
    TConnection* result = nullptr;
    if (connectionStack_.empty()) {
        result = new TConnection(socket, ioThread, addr, addrLen);
        ++numTConnections_;
    } else {
        result = connectionStack_.top();
        connectionStack_.pop();
        result->init(socket, ioThread, addr, addrLen);
    }
    activeConnections_.push_back(result);
    return result;
}

/**
 * Returns a connection to the stack
 */
void TNonblockingServer::returnConnection(TConnection* connection) {
    Guard g(connMutex_);

    activeConnections_.erase(std::remove(activeConnections_.begin(),
                                         activeConnections_.end(),
                                         connection),
                             activeConnections_.end());

    if (connectionStackLimit_ && (connectionStack_.size() >= connectionStackLimit_)) {
        delete connection;
        --numTConnections_;
    } else {
        connection->checkIdleBufferMemLimit(idleReadBufferLimit_, idleWriteBufferLimit_);
        connectionStack_.push(connection);
    }
}

/**
 * Server socket had something happen.  We accept all waiting client
 * connections on fd and assign TConnection objects to handle those requests.
 */
void TNonblockingServer::handleEvent(THRIFT_SOCKET fd, short which) {
    (void)which;
    if ( fd != serverSocket_ ) {
        GlobalOutput.printf(" fd: %d != serverSocket_: %d fatal error!!!", fd, serverSocket_);
//        return ;
    }
    // Make sure that libevent didn't mess up the socket handles
    assert(fd == serverSocket_);

    // Server socket accepted a new connection
    socklen_t addrLen;
    sockaddr_storage addrStorage;
    sockaddr* addrp = (sockaddr*)&addrStorage;
    addrLen = sizeof(addrStorage);

    // Going to accept a new client socket
    THRIFT_SOCKET clientSocket;

    // Accept as many new clients as possible, even though libevent signaled only
    // one, this helps us to avoid having to go back into the libevent engine so
    // many times
    while ((clientSocket = ::accept(fd, addrp, &addrLen)) != -1) {
        // If we're overloaded, take action here
        if (overloadAction_ != T_OVERLOAD_NO_ACTION && serverOverloaded()) {
            Guard g(connMutex_);
            nConnectionsDropped_++;
            nTotalConnectionsDropped_++;
            if (overloadAction_ == T_OVERLOAD_CLOSE_ON_ACCEPT) {
                ::THRIFT_CLOSESOCKET(clientSocket);
                return;
            } else if (overloadAction_ == T_OVERLOAD_DRAIN_TASK_QUEUE) {
                if (!drainPendingTask()) {
                    // Nothing left to discard, so we drop connection instead.
                    ::THRIFT_CLOSESOCKET(clientSocket);
                    return;
                }
            }
        }

        // Explicitly set this socket to NONBLOCK mode
        int flags;
        if ((flags = THRIFT_FCNTL(clientSocket, THRIFT_F_GETFL, 0)) < 0
                || THRIFT_FCNTL(clientSocket, THRIFT_F_SETFL, flags | THRIFT_O_NONBLOCK) < 0) {
            GlobalOutput.perror("thriftServerEventHandler: set THRIFT_O_NONBLOCK (THRIFT_FCNTL) ",
                                THRIFT_GET_SOCKET_ERROR);
            ::THRIFT_CLOSESOCKET(clientSocket);
            return;
        }

        // Create a new TConnection for this client socket.
        TConnection* clientConnection = createConnection(clientSocket, addrp, addrLen);

        // Fail fast if we could not create a TConnection object
        if (clientConnection == NULL) {
            GlobalOutput.printf("thriftServerEventHandler: failed TConnection factory");
            ::THRIFT_CLOSESOCKET(clientSocket);
            return;
        }

        /*
             * Either notify the ioThread that is assigned this connection to
             * start processing, or if it is us, we'll just ask this
             * connection to do its initial state change here.
             *
             * (We need to avoid writing to our own notification pipe, to
             * avoid possible deadlocks if the pipe is full.)
             *
             * The IO thread #0 is the only one that handles these listen
             * events, so unless the connection has been assigned to thread #0
             * we know it's not on our thread.
         */
        // 当0号iothread 监听到accept事件时，创建connection 并交给相应的iothread处理数据收发(通过管道方式通知相应的iothread)
        if (clientConnection->getIOThreadNumber() == 0) {// 侦听线程不应该处理网络IO
            GlobalOutput.printf("getIOThreadNumber() == 0 beg------");
            clientConnection->transition();
            GlobalOutput.printf("getIOThreadNumber() == 0 end------");
        } else {
            GlobalOutput.printf("notifyiothread beg------dst iothread number=%d", clientConnection->getIOThreadNumber());
            // 把 newConnection 对象, 投递给 iothread 线程, 此处我们考虑用更高性能方式代替poll
            if (!clientConnection->notifyIOThread()) {
                GlobalOutput.perror("[ERROR] notifyIOThread failed on fresh connection, closing", errno);
                returnConnection(clientConnection);
            }
            GlobalOutput.printf("notifyiothread end------");
        }

        // addrLen is written by the accept() call, so needs to be set before the next call.
        addrLen = sizeof(addrStorage);
    }

    // Done looping accept, now we have to make sure the error is due to
    // blocking. Any other error is a problem
    if (THRIFT_GET_SOCKET_ERROR != THRIFT_EAGAIN && THRIFT_GET_SOCKET_ERROR != THRIFT_EWOULDBLOCK) {
        GlobalOutput.perror("thriftServerEventHandler: accept() ", THRIFT_GET_SOCKET_ERROR);
    }
}

/**
 * Creates a socket to listen on and binds it to the local port.
 */
void TNonblockingServer::createAndListenOnSocket() {
    THRIFT_SOCKET s;

    struct addrinfo hints, *res, *res0;
    int error;

    char port[sizeof("65536") + 1];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    sprintf(port, "%d", port_);

    // Wildcard address
    error = getaddrinfo(NULL, port, &hints, &res0);
    if (error) {
        throw TException("TNonblockingServer::serve() getaddrinfo "
                         + string(THRIFT_GAI_STRERROR(error)));
    }

    // Pick the ipv6 address first since ipv4 addresses can be mapped
    // into ipv6 space.
    for (res = res0; res; res = res->ai_next) {
        if (res->ai_family == AF_INET6 || res->ai_next == NULL)
            break;
    }

    // Create the server socket
    //    printf( "res->ai_family=%s\n", /*res->ai_family,*/ (res->ai_family == PF_INET) ? "PF_INET" : "unknown type");
    GlobalOutput.printf( "res->ai_family=%s", ((res->ai_family == PF_INET) ? "PF_INET" : "unknown type") );
    s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == -1) {
        freeaddrinfo(res0);
        throw TException("TNonblockingServer::serve() socket() -1");
    }

#ifdef IPV6_V6ONLY
    if (res->ai_family == AF_INET6) {
        int zero = 0;
        if (-1 == setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, const_cast_sockopt(&zero), sizeof(zero))) {
            GlobalOutput("TServerSocket::listen() IPV6_V6ONLY");
        }
    }
#endif // #ifdef IPV6_V6ONLY

    int one = 1;

    // Set THRIFT_NO_SOCKET_CACHING to avoid 2MSL delay on server restart
    setsockopt(s, SOL_SOCKET, THRIFT_NO_SOCKET_CACHING, const_cast_sockopt(&one), sizeof(one));

    if (::bind(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) == -1) {
        ::THRIFT_CLOSESOCKET(s);
        freeaddrinfo(res0);
        throw TTransportException(TTransportException::NOT_OPEN,
                                  "TNonblockingServer::serve() bind",
                                  THRIFT_GET_SOCKET_ERROR);
    }

    // Done with the addr info
    freeaddrinfo(res0);

    // Set up this file descriptor for listening
    listenSocket(s);
}

/**
 * Takes a socket created by listenSocket() and sets various options on it
 * to prepare for use in the server.
 */
void TNonblockingServer::listenSocket(THRIFT_SOCKET s) {
    // Set socket to nonblocking mode
    int flags;
    if ((flags = THRIFT_FCNTL(s, THRIFT_F_GETFL, 0)) < 0
            || THRIFT_FCNTL(s, THRIFT_F_SETFL, flags | THRIFT_O_NONBLOCK) < 0) {
        ::THRIFT_CLOSESOCKET(s);
        throw TException("TNonblockingServer::serve() THRIFT_O_NONBLOCK");
    }

    int one = 1;
    struct linger ling = {0, 0};

    // Keepalive to ensure full result flushing
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, const_cast_sockopt(&one), sizeof(one));

    // Turn linger off to avoid hung sockets
    setsockopt(s, SOL_SOCKET, SO_LINGER, const_cast_sockopt(&ling), sizeof(ling));

    // Set TCP nodelay if available, MAC OS X Hack
    // See http://lists.danga.com/pipermail/memcached/2005-March/001240.html
#ifndef TCP_NOPUSH
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, const_cast_sockopt(&one), sizeof(one));
#endif

#ifdef TCP_LOW_MIN_RTO
    if (TSocket::getUseLowMinRto()) {
        setsockopt(s, IPPROTO_TCP, TCP_LOW_MIN_RTO, const_cast_sockopt(&one), sizeof(one));
    }
#endif

    if (listen(s, LISTEN_BACKLOG) == -1) {
        ::THRIFT_CLOSESOCKET(s);
        throw TException("TNonblockingServer::serve() listen");
    }

    // Cool, this socket is good to go, set it as the serverSocket_
    serverSocket_ = s;

    if (!port_) {
        sockaddr_in addr;
        socklen_t size = sizeof(addr);
        if (!getsockname(serverSocket_, reinterpret_cast<sockaddr*>(&addr), &size)) {
            listenPort_ = ntohs(addr.sin_port);
        } else {
            GlobalOutput.perror("TNonblocking: failed to get listen port: ", THRIFT_GET_SOCKET_ERROR);
        }
    }
}

void TNonblockingServer::setThreadManager(boost::shared_ptr<ThreadManager> threadManager) {
    threadManager_ = threadManager;
    if (threadManager) {
        threadManager->setExpireCallback(
                    apache::thrift::stdcxx::bind(&TNonblockingServer::expireClose,
                                                 this,
                                                 apache::thrift::stdcxx::placeholders::_1));
        threadPoolProcessing_ = true;
    } else {
        threadPoolProcessing_ = false;
    }
}

bool TNonblockingServer::serverOverloaded() {
    size_t activeConnections = numTConnections_ - connectionStack_.size();
    if (numActiveProcessors_ > maxActiveProcessors_ || activeConnections > maxConnections_) {
        if (!overloaded_) {
            GlobalOutput.printf("TNonblockingServer: overload condition begun.");
            overloaded_ = true;
        }
    } else {
        if (overloaded_ && (numActiveProcessors_ <= overloadHysteresis_ * maxActiveProcessors_)
                && (activeConnections <= overloadHysteresis_ * maxConnections_)) {
            GlobalOutput.printf(
                        "TNonblockingServer: overload ended; "
                        "%u dropped (%llu total)",
                        nConnectionsDropped_,
                        nTotalConnectionsDropped_);
            nConnectionsDropped_ = 0;
            overloaded_ = false;
        }
    }

    return overloaded_;
}

bool TNonblockingServer::drainPendingTask() {
    if (threadManager_) {
        boost::shared_ptr<Runnable> task = threadManager_->removeNextPending();
        if (task) {
            TConnection* connection = static_cast<TConnection::Task*>(task.get())->getTConnection();
            assert(connection && connection->getServer() && connection->getState() == APP_WAIT_TASK);
            connection->forceClose();
            return true;
        }
    }
    return false;
}

void TNonblockingServer::expireClose(boost::shared_ptr<Runnable> task) {
    TConnection* connection = static_cast<TConnection::Task*>(task.get())->getTConnection();
    assert(connection && connection->getServer() && connection->getState() == APP_WAIT_TASK);
    connection->forceClose();
}

void TNonblockingServer::stop() {
    if (!port_) {
        listenPort_ = 0;
    }
    // Breaks the event loop in all threads so that they end ASAP.
    for (uint32_t i = 0; i < ioThreads_.size(); ++i) {
        ioThreads_[i]->stop();
    }
}

void TNonblockingServer::registerEvents(event_base* user_event_base) {
    userEventBase_ = user_event_base;

    // init listen socket
    if (serverSocket_ == THRIFT_INVALID_SOCKET)
        createAndListenOnSocket();

    // set up the IO threads
    assert(ioThreads_.empty());
//    if (!numIOThreads_) {
//        numIOThreads_ = DEFAULT_IO_THREADS;
//    }
    numIOThreads_ = DEFAULT_IO_THREADS;

    // User-provided event-base doesn't works for multi-threaded servers
    assert(numIOThreads_ == 1 || !userEventBase_);

    m_listenThread.reset( new TNonblockingIOThread( this, 0, serverSocket_, useHighPriorityIOThreads_ ) );

    GlobalOutput.printf( "numIOThreads_: %d created", numIOThreads_ );
    for (uint32_t id = 0; id < numIOThreads_; ++id) {
        // the first IO thread also does the listening on server socket
//        THRIFT_SOCKET listenFd = (id == 0 ? serverSocket_ : THRIFT_INVALID_SOCKET);

        shared_ptr<TNonblockingIOThread> thread(new TNonblockingIOThread(this, (id+1)/*id*/, -1/*listenFd*/, useHighPriorityIOThreads_));
        ioThreads_.push_back(thread);
//        GlobalOutput.printf("new iothread: %d created", CurTid(), thread->GetCurIoThreadId() );
    }

    if (eventHandler_) {// Notify handler of the preServe event
        eventHandler_->preServe();
    }

    // Start all of our helper IO threads. Note that the threads run forever,
    // only terminating if stop() is called.
    assert(ioThreads_.size() == numIOThreads_);
    assert(ioThreads_.size() > 0);

    GlobalOutput.printf("TNonblockingServer: Serving on port %d, %d io threads.", listenPort_, ioThreads_.size());

    // Launch all the secondary IO threads in separate threads
    if (ioThreads_.size() > 1) {
        ioThreadFactory_.reset(new PlatformThreadFactory(
                           #if !USE_BOOST_THREAD && !USE_STD_THREAD
                                   PlatformThreadFactory::OTHER,  // scheduler
                                   PlatformThreadFactory::NORMAL, // priority
                                   1,                             // stack size (MB)
                           #endif
                                   false // detached
                                   ));
        assert(ioThreadFactory_.get());

        // intentionally starting at thread 1, not 0
        for (uint32_t i = 0/*1*/; i < ioThreads_.size(); ++i) {
            shared_ptr<Thread> thread = ioThreadFactory_->newThread(ioThreads_[i]);
            ioThreads_[i]->setThread(thread);
            thread->start();
        }
    }

    // Register the events for the primary (listener) IO thread
//    ioThreads_[0]->register_io_events();
    m_listenThread->register_io_events();
}

/**
 * Main workhorse function, starts up the server listening on a port and
 * loops over the libevent handler.
 */
// 创建socket监听 创建TNonblockingIOThread 通过Thread启动TNonblockingIOThread
void TNonblockingServer::serve() {
    if (ioThreads_.empty())
        registerEvents(nullptr);

    // Run the primary (listener) IO thread loop in our main thread; this will
    // only return when the server is shutting down.
//    ioThreads_[0]->run();

    // new add
    m_listenThread->run();

    // Ensure all threads are finished before exiting serve()
    for (uint32_t i = 0; i < ioThreads_.size(); ++i) {
        ioThreads_[i]->join();
        GlobalOutput.printf("TNonblocking: join done for IO thread #%d", i);
    }
}

TNonblockingIOThread::TNonblockingIOThread(TNonblockingServer* server,
                                           int number,
                                           THRIFT_SOCKET listenSocket,
                                           bool useHighPriority)
    : server_(server),
      number_(number),
      listenSocket_(listenSocket),
      useHighPriority_(useHighPriority),
      eventBase_(NULL),
      ownEventBase_(false) {
    notificationPipeFDs_[0] = -1;
    notificationPipeFDs_[1] = -1;
}

TNonblockingIOThread::~TNonblockingIOThread() {
    // make sure our associated thread is fully finished
    join();

    if (eventBase_ && ownEventBase_) {
        event_base_free(eventBase_);
        ownEventBase_ = false;
    }

    if (listenSocket_ >= 0) {
        if (0 != ::THRIFT_CLOSESOCKET(listenSocket_)) {
            GlobalOutput.perror("TNonblockingIOThread listenSocket_ close(): ", THRIFT_GET_SOCKET_ERROR);
        }
        listenSocket_ = THRIFT_INVALID_SOCKET;
    }

    for (int i = 0; i < 2; ++i) {
        if (notificationPipeFDs_[i] >= 0) {
            if (0 != ::THRIFT_CLOSESOCKET(notificationPipeFDs_[i])) {
                GlobalOutput.perror("TNonblockingIOThread notificationPipe close(): ",
                                    THRIFT_GET_SOCKET_ERROR);
            }
            notificationPipeFDs_[i] = THRIFT_INVALID_SOCKET;
        }
    }
}

void TNonblockingIOThread::createNotificationPipe() {
    if (evutil_socketpair(AF_LOCAL, SOCK_STREAM, 0, notificationPipeFDs_) == -1) {
        GlobalOutput.perror("TNonblockingServer::createNotificationPipe ", EVUTIL_SOCKET_ERROR());
        throw TException("can't create notification pipe");
    }
    if (evutil_make_socket_nonblocking(notificationPipeFDs_[0]) < 0 ||
        evutil_make_socket_nonblocking(notificationPipeFDs_[1]) < 0) {
        ::THRIFT_CLOSESOCKET(notificationPipeFDs_[0]);
        ::THRIFT_CLOSESOCKET(notificationPipeFDs_[1]);
        throw TException("TNonblockingServer::createNotificationPipe() THRIFT_O_NONBLOCK");
    }
    for (int i = 0; i < 2; ++i) {
#if LIBEVENT_VERSION_NUMBER < 0x02000000
        int flags;
        if ((flags = THRIFT_FCNTL(notificationPipeFDs_[i], F_GETFD, 0)) < 0
                || THRIFT_FCNTL(notificationPipeFDs_[i], F_SETFD, flags | FD_CLOEXEC) < 0) {
#else
        if (evutil_make_socket_closeonexec(notificationPipeFDs_[i]) < 0) {
#endif
            ::THRIFT_CLOSESOCKET(notificationPipeFDs_[0]);
            ::THRIFT_CLOSESOCKET(notificationPipeFDs_[1]);
            throw TException(
                        "TNonblockingServer::createNotificationPipe() "
                        "FD_CLOEXEC");
        }
    }
}

/**
 * Register the core libevent events onto the proper base.
 */
void TNonblockingIOThread::register_io_events() {
    threadId_ = Thread::get_current();

    assert(eventBase_ == nullptr);
    eventBase_ = getServer()->getUserEventBase();
    if (eventBase_ == nullptr) {
        eventBase_ = event_base_new();
        ownEventBase_ = true;
    }

//    if (number_ == 0) {// Print some libevent stats
        GlobalOutput.printf("TNonblockingServer: using libevent %s method %s",
                            event_get_version(), event_base_get_method(eventBase_));
//    }

    if (listenSocket_ >= 0) {
        // Register the server event
        // 当新的连接请求进来的时候，TNonblockingIOThread::listenHandler 函数被触发，
        // 在 TNonblockingIOThread::listenHandler 里主要负责 accept 新连接。
        // listenHandler和listensocket关联
        event_set(&serverEvent_, listenSocket_,
                  EV_READ | EV_PERSIST,
                  TNonblockingIOThread::listenHandler, server_);
        event_base_set(eventBase_, &serverEvent_);

        // Add the event and start up the server
        if (-1 == event_add(&serverEvent_, 0)) {
            throw TException("TNonblockingServer::serve(): event_add() failed on server listen event");
        }
        GlobalOutput.printf("TNonblocking: IO thread #%d registered for listen. listenSocket_=%d", number_, listenSocket_);
    }

    createNotificationPipe();

    // 代码里面的 getNotificationRecvFD 就是拿这个 socket pair 管道的读文件描述符，
    // 也就是当这个 socketpair 管道有数据可读时，该事件就会被触发，
    // 也就是回调函数 TNonblockingIOThread::notifyHandler 会被调用。
    // 其实第二种事件非常好理解，可以类比多线程编程里面的任务队列，
    // 不同线程之间共享着同一个任务队列来进行消息的传递。 而在 TNonblockingServer 里面，则通过该管道进行消息的传递。
    event_set(&notificationEvent_, getNotificationRecvFD(),
              EV_READ | EV_PERSIST, TNonblockingIOThread::notifyHandler, this);
    event_base_set(eventBase_, &notificationEvent_);// Attach to the base

    if (-1 == event_add(&notificationEvent_, 0)) {// Add the event and start up the server
        throw TException("TNonblockingServer::serve(): event_add() failed on task-done notification event");
    }
    GlobalOutput.printf( "%s registered for notify.", (listenSocket_ >= 0 ? ("listen thread") : ("work iothread")) );

//    auto tmpFuncPtr = boost::bind(&TNonblockingIOThread::notifyHandler, _1, _2, _3);
//    watcher_notify_.reset( new PipeEventWatcher(eventBase_, tmpFuncPtr) );
//    watcher_notify_->Init();
//    watcher_notify_->AsyncWait();
//    GlobalOutput.printf( "%s registered for notify.", (listenSocket_ >= 0 ? ("listen thread") : ("work iothread")) );

//    watcher_notify_->Notify();
}

bool TNonblockingIOThread::notify(TNonblockingServer::TConnection* conn) {
    THRIFT_SOCKET fd = getNotificationSendFD();
    if (fd < 0) {
        return false;
    }

    int ret = -1;
    int kSize = sizeof(conn);
    const char* pos = (const char*)const_cast_sockopt(&conn);
#ifndef __linux__
    printf( "TNonblockingIOThread::notify select logic\n" );
    fd_set wfds, efds;
    while (kSize > 0) {
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        FD_SET(fd, &wfds);
        FD_SET(fd, &efds);
        ret = select(fd + 1, NULL, &wfds, &efds, NULL);
        if (ret < 0) {
            return false;
        } else if (ret == 0) {
            continue;
        }

        if (FD_ISSET(fd, &efds)) {
            ::THRIFT_CLOSESOCKET(fd);
            return false;
        }

        if (FD_ISSET(fd, &wfds)) {
            ret = send(fd, pos, kSize, 0);
            if (ret < 0) {
                if (errno == EAGAIN) {
                    continue;
                }

                ::THRIFT_CLOSESOCKET(fd);
                return false;
            }

            kSize -= ret;
            pos += ret;
        }
    }
#else
    GlobalOutput.printf("TNonblockingIOThread::notify poll logic");
    struct pollfd pfd = {fd, POLLOUT, 0};
    while (kSize > 0) {
        pfd.revents = 0;
        ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            return false;
        } else if (ret == 0) {
            continue;
        }

        if (pfd.revents & POLLHUP || pfd.revents & POLLERR) {
            ::THRIFT_CLOSESOCKET(fd);
            return false;
        }

        if (pfd.revents & POLLOUT) {
            ret = send(fd, pos, kSize, 0);
            if (ret < 0) {
                if (errno == EAGAIN) {
                    continue;
                }
                ::THRIFT_CLOSESOCKET(fd);
                return false;
            }
            kSize -= ret;
            pos += ret;
        }
    }
#endif

    return true;
}

/* static */
// iothread收到新的connection根据connection的状态 进行数据收发等处理 ，该逻辑由conection的
// transition()函数完成, 第一步肯定是请求报文read操作
// iothread获得pipefd事件后调用，读取一个指针的大小，获得connection后调用transition.
void TNonblockingIOThread::notifyHandler(evutil_socket_t fd, short which, void* v) {
    TNonblockingIOThread* ioThread = (TNonblockingIOThread*)v;
    assert(ioThread);
    (void)which;

    GlobalOutput.printf("iothread notifyHandler exec!!!");
    while (true) {
        TNonblockingServer::TConnection* connection = nullptr;
        const int kSize = sizeof(connection);
        // 从管道中取出connection的指针地址
        long nBytes = recv(fd, cast_sockopt(&connection), kSize, 0);
        if (nBytes == kSize) {
            if (connection == nullptr) {
                // this is the command to stop our thread, exit the handler!
//                ioThread->breakLoop(false);
                return;
            }
            connection->transition();// 进入状态转换函数
        } else if (nBytes > 0) {
            // throw away these bytes and hope that next time we get a solid read
            GlobalOutput.printf("notifyHandler: Bad read of %d bytes, wanted %d", nBytes, kSize);
            ioThread->breakLoop(true);
            return;
        } else if (nBytes == 0) {
            GlobalOutput.printf("notifyHandler: Notify socket closed!");
//            ioThread->breakLoop(false);
            // exit the loop
            break;
        } else { // nBytes < 0
            if (THRIFT_GET_SOCKET_ERROR != THRIFT_EWOULDBLOCK &&
                THRIFT_GET_SOCKET_ERROR != THRIFT_EAGAIN) {
                GlobalOutput.perror("TNonblocking: notifyHandler read() failed: ", THRIFT_GET_SOCKET_ERROR);
                ioThread->breakLoop(true);
                return;
            }
            // exit the loop
            break;
        }
    }
}

void TNonblockingIOThread::breakLoop(bool error) {
    if (error) {
        GlobalOutput.printf("TNonblockingServer: IO thread #%d exiting with error.", number_);
        // TODO: figure out something better to do here, but for now kill the
        // whole process.
        GlobalOutput.printf("TNonblockingServer: aborting process.");
        ::abort();
    }

    // sets a flag so that the loop exits on the next event
    // 需要判断是否在当前iothread内执行，跨线程不能执行base_loopbreak
//    event_base_loopbreak(eventBase_);

    // event_base_loopbreak() only causes the loop to exit the next time
    // it wakes up.  We need to force it to wake up, in case there are
    // no real events it needs to process.
    //
    // If we're running in the same thread, we can't use the notify(0)
    // mechanism to stop the thread, but happily if we're running in the
    // same thread, this means the thread can't be blocking in the event
    // loop either.
    if (!Thread::is_current(threadId_)) {
        notify(nullptr);
    } else {
        // cause the loop to stop ASAP - even if it has things to do in it
        event_base_loopbreak(eventBase_);
    }
}

void TNonblockingIOThread::setCurrentThreadHighPriority(bool value) {
#ifdef HAVE_SCHED_H
    // Start out with a standard, low-priority setup for the sched params.
    struct sched_param sp;
    bzero((void*)&sp, sizeof(sp));
    int policy = SCHED_OTHER;

    // If desired, set up high-priority sched params structure.
    if (value) {
        // FIFO scheduler, ranked above default SCHED_OTHER queue
        policy = SCHED_FIFO;
        // The priority only compares us to other SCHED_FIFO threads, so we
        // just pick a random priority halfway between min & max.
        const int priority = (sched_get_priority_max(policy) + sched_get_priority_min(policy)) / 2;

        sp.sched_priority = priority;
    }

    // Actually set the sched params for the current thread.
    if (0 == pthread_setschedparam(pthread_self(), policy, &sp)) {
        GlobalOutput.printf("TNonblocking: IO Thread #%d using high-priority scheduler!", number_);
    } else {
        GlobalOutput.perror("TNonblocking: pthread_setschedparam(): ", THRIFT_GET_SOCKET_ERROR);
    }
#else
    THRIFT_UNUSED_VARIABLE(value);
#endif
}

void TNonblockingIOThread::run() {
    if (eventBase_ == nullptr)
        register_io_events();

//    GlobalOutput.printf("TNonblockingServer: IO thread #%d entering loop...", number_);
//    GlobalOutput.printf("new iothread: %d running", CurTid());
    if (useHighPriority_) {
        setCurrentThreadHighPriority(true);
    }

    if (eventBase_ != nullptr) {
        // Run libevent engine, never returns, invokes calls to eventHandler
        event_base_loop(eventBase_, 0);

        if (useHighPriority_) {
            setCurrentThreadHighPriority(false);
        }

        // cleans up our registered events
        cleanupEvents();
    }

    GlobalOutput.printf("TNonblockingServer: IO thread #%d run() done!", number_);
}

void TNonblockingIOThread::cleanupEvents() {
    // stop the listen socket, if any
    if (listenSocket_ >= 0) {
        if (event_del(&serverEvent_) == -1) {
            GlobalOutput.perror("TNonblockingIOThread::stop() event_del: ", THRIFT_GET_SOCKET_ERROR);
        }
    }
    event_del(&notificationEvent_);
}

void TNonblockingIOThread::stop() {
    // This should cause the thread to fall out of its event loop ASAP.
    breakLoop(false);
}

void TNonblockingIOThread::join() {
    // If this was a thread created by a factory (not the thread that called
    // serve()), we join() it to make sure we shut down fully.
    if (thread_) {
        try {
            // Note that it is safe to both join() ourselves twice, as well as join
            // the current thread as the pthread implementation checks for deadlock.
            thread_->join();
        } catch (...) {
            // swallow everything
        }
    }
}

int TNonblockingIOThread::GetCurIoThreadId() {
    return muduo::CurrentThread::tid();
}

}
}
} // apache::thrift::server
