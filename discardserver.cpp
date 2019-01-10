#include <muduo/net/TcpServer.h>


#include <boost/bind.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>


#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include "muduo/base/ThreadPool.h"

#include "muduo/net/EventLoopThreadPool.h"



using namespace muduo;
using namespace muduo::net;


int numThreads = 0;


class DiscardServer
{
public:
    DiscardServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "DiscardServer"),
          oldCounter_(0),
          startTime_(Timestamp::now())
    {
        server_.setConnectionCallback(
                    boost::bind(&DiscardServer::onConnection, this, _1));
        server_.setMessageCallback(
                    boost::bind(&DiscardServer::onMessage, this, _1, _2, _3));
        server_.setThreadNum(numThreads);

//        loop->runEvery(3.0, boost::bind(&DiscardServer::printThroughput, this));
    }

    void start() {
        LOG_INFO << "starting " << numThreads << " threads.";
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
                  << conn->localAddress().toIpPort() << " is "
                  << (conn->connected() ? "UP" : "DOWN");
    }

// 这是用户提供给TcpServer的可读时的回调函数，
//又由TcpServer提供给TcpConnection，当TcpConnection的读缓冲区执行完readFd返回后，会执行用户的回调函数，图片程序来自muduo的测试用例。
/*
 * 可以看到
    buf->readableBytes()返回缓冲区中可读字节数
    conn->name()返回TcpConnection的名字（由TcpServer设置）
    receiveTime是poll函数返回的时间，一直作为参数传到Channel，TcpConnection，onMessage
    buf->retrieveAsString()读取缓冲区所有数据
*/
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        size_t len = buf->readableBytes();
        LOG_INFO << "recv len=" << len << ", recvData=" << buf->retrieveAsString(len);
        transferred_.add(len);
        receivedMessages_.incrementAndGet();
        buf->retrieveAll();
    }

    void printThroughput()
    {
        Timestamp endTime = Timestamp::now();

        int64_t newCounter = transferred_.get();
        int64_t bytes = newCounter - oldCounter_;
        int64_t msgs = receivedMessages_.getAndSet(0);

        double time = timeDifference(endTime, startTime_);
        printf("%4.3f MiB/s %4.3f Ki Msgs/s %6.2f bytes per msg\n",
               static_cast<double>(bytes)/time/1024/1024,
               static_cast<double>(msgs)/time/1024,
               static_cast<double>(bytes)/static_cast<double>(msgs));

        oldCounter_ = newCounter;
        startTime_  = endTime;
    }

    TcpServer server_;

    AtomicInt64 transferred_;
    AtomicInt64 receivedMessages_;
    int64_t oldCounter_;
    Timestamp startTime_;
};

//int main(int argc, char* argv[])
int tst_discard_server()
{
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

//    if (argc > 1)
//    {
        numThreads = 2;
//    }

    EventLoop loop;
    InetAddress listenAddr(2009);
    DiscardServer server(&loop, listenAddr);

    server.start();

    loop.loop();
}







class CTestServer {

public:
    CTestServer( EventLoop* loop, const InetAddress &listenAddr )
        : loop_(loop),
          server_(loop, listenAddr, "testserver")
    {

        server_.setConnectionCallback(
                    boost::bind(&CTestServer::onConnection, this, _1) );
        server_.setMessageCallback(
                    boost::bind(&CTestServer::onMessage, this, _1, _2, _3) );
//        server_.setWriteCompleteCallback(
//                    boost::bind(&CTestServer::onWriteComplete, this, _1));

        // 生成数据
        string line;
        for (int i = 33; i < 127; ++i)
        {
            line.push_back(char(i));
        }
        line += line;

//        printf("line=%s\n", line.c_str());
        for (size_t i = 0; i < 127 - 33; ++i)
        {
            message_ += line.substr(i, 72) + '\n';
        }
        message_ = "12345";
        server_.setThreadNum(1);
//        printf("message_=%s\n", message_.c_str());

//        exit(1);
//        server_.setThreadNum(1);

//        message1_.resize(10);
////        message2_.resize(200);
//        std::fill(message1_.begin(), message1_.end(), 'A');
////        std::fill(message2_.begin(), message2_.end(), 'B');

    }
    void start( ) {
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn) {

        if (conn->connected()) {
            printf("onConnection(): new connection [%s] from %s\n",
                   conn->name().c_str(),
                   conn->peerAddress().toIpPort().c_str());

            conn->setTcpNoDelay(true);
//            conn->send(message_);
//            conn->shutdown();
        } else {
            printf("onConnection(): connection [%s] is down\n",
                   conn->name().c_str());
        }

    }
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp receiveTime)
    {
        muduo::string msg(buf->retrieveAllAsString());
        printf("onMessage(): received %zd bytes from connection [%s] at %s\n",
               msg.size(),
               conn->name().c_str(),
               receiveTime.toFormattedString().c_str());
        conn->send(msg);
    }
    void onWriteComplete(const TcpConnectionPtr& conn) {
        conn->send(message_);
    }

private:
    EventLoop *loop_;
    TcpServer server_;

    muduo::string message_;
//    muduo::string message1_;
//    muduo::string message2_;

};

void tst_Tcp_server1() {
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

    InetAddress listenAddr(8888);
    EventLoop loop;

    CTestServer server(&loop, listenAddr);
    server.start();

    loop.loop();
}



muduo::MutexLock g_mtx_tasks;

//std::vector<int> g_vec_tasks;

typedef std::vector<int> vectInts;

boost::shared_ptr<vectInts> g_pTasks;

void  readTask() {

//    LOG_INFO << "readTask tid=" << CurrentThread::tid();
//    printf( "readTask tid=%d\n", CurrentThread::tid() );

    boost::shared_ptr<vectInts> tmpPtr;
    {
        MutexLockGuard lock(g_mtx_tasks);
        tmpPtr = g_pTasks;
    }
//    LOG_INFO << "readTask tmp use_cout=" << tmpPtr.use_count();
    printf( "readTask tid=%d tmp use_cout=%d\n", CurrentThread::tid(), tmpPtr.use_count() );
    // for read oper
}

void  writeTask( /*const int& oneTask*/ ) {
    LOG_INFO << "writeTask tid=" << CurrentThread::tid();

    for ( int i = 0; i < 1000; ++ i ) {
        sleep( 2 );
    }
}

void tst_mt_cow() {
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

    Thread t1( boost::bind(&readTask), "readthread" );
    Thread t2( boost::bind(&readTask), "readthread" );

    std::vector<int>* tasklist = new std::vector<int>;
    g_pTasks.reset(tasklist);
//    g_pTasks->reserve( 20000 );
    for ( int i = 0; i < 1000; ++ i ) {
        g_pTasks->push_back(i);
    }
    LOG_INFO << "size=" << g_pTasks->size() << ", cap=" << g_pTasks->capacity();

    t1.start();
    t2.start();

    t1.join();
    printf( "tttt use_cout=%d\n", g_pTasks.use_count() );
    t2.join();
    printf( "uuuu use_cout=%d\n", g_pTasks.use_count() );
    sleep( 2 );
    return;

    EventLoop mainEl;

    EventLoopThreadPool readPool(&mainEl,  /*std::string*/("readpool"));
    EventLoopThreadPool writePool(&mainEl, /*std::string*/("writePool"));

    readPool.setThreadNum(2);
    writePool.setThreadNum(1);

    readPool.start();

    mainEl.loop();
}








