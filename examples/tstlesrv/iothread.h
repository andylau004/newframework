
#pragma once



#include <stdio.h>

#include <queue>


#include <atomic>

#include <event.h>


#include <thrift/protocol/TProtocol.h>


#include "muduo/base/common.h"
#include "common_stucts.hpp"


#include "muduo/base/Logging.h"

#include "muduo/net/InetAddress.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/Logging.h"
#include "muduo/net/TcpClient.h"

#include "../../muduo/base/event_watcher.h"


using namespace std;
using namespace muduo;
using namespace muduo::net;

using namespace boost;




typedef struct conn_queue_s {
    MutexLock               m_newlock;
    std::deque<Item_t *>    m_connQueue;

    conn_queue_s()  {}
    ~conn_queue_s() {}

} conn_queue_t;



class ClientCtx;
class CEventLoopThread;

class CIOThread {
    typedef std::queue<ClientCtx*> CCQ;

public:
    CIOThread(int id);
    ~CIOThread();

public:
    int     get_tid() const             { return m_tid; }
    bool    start();


    bool    set_item(Item_t * item);
    Item_t* get_connitem();

    void    return_clientctx(ClientCtx * ctx);
    void    setPushConnection(int64_t id, ClientCtx *ctx);
    void    deletePushConnection(int64_t id, ClientCtx * ctx);

    CEventLoopThread* get_evlthread()   { return m_elthread; }

private:
    ClientCtx* get_clientctx();
    bool       create_connection(Item_t* newItem);

private:
    bool init(int tid);
    bool register_notifyevent();
    bool register_timerevent();

    static void thread_handle_newconnection(int fd, short which, void *arg, void *user_data);

private:
    int  m_tid;
    // 封装 替代 notificationPipeFDs_
    PipeEventWatcherPtr notify_watcher_;

    conn_queue_t*       m_newConns;
    CEventLoopThread*   m_elthread;

    CCQ         m_idleCtxs;

    boost::unordered_map<int64_t, ClientCtx*> pushConnections_;

};



