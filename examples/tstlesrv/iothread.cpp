


#include "iothread.h"


#include "eventloopthread.h"

#include "muduo/base/common.h"

#include "muduo/net/InetAddress.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/TcpServer.h"
#include "muduo/base/Logging.h"
#include "muduo/net/TcpClient.h"

#include "../../muduo/base/event_watcher.h"

#include "clientctx.hpp"


CIOThread::CIOThread(int tid) :
    m_tid(tid),
    m_newConns(nullptr),
    m_elthread(nullptr)

{

}

CIOThread::~CIOThread() {
    delete_object(m_newConns);
    delete_object(m_elthread);
}

bool CIOThread::set_item(Item_t * item) {
    if (item == nullptr || m_newConns == nullptr) {
        return false;
    }
    {
        MutexLockGuard lock(m_newConns->m_newlock);
        m_newConns->m_connQueue.push_back(item);
    }
    notify_watcher_->Notify();
    return true;
}
bool CIOThread::init(int tid) {
    if (!m_newConns) {
        m_newConns = new conn_queue_t;
    }
    if (!m_elthread) {
        m_elthread = new CEventLoopThread(tid);
    }
    if (!register_notifyevent()) {
        LOG_ERROR << "register notify event failed";
        return false;
    }
    return true;
}
bool CIOThread::start() {
    if (!init(m_tid))
        return false;
    return m_elthread->start(true) == SUCCESS;
}
void CIOThread::return_clientctx(ClientCtx* ctx) {
    if (ctx == NULL) {
        return;
    }
    ctx->set_idle_time();
    m_idleCtxs.push(ctx);
    LOG_INFO << " ctx=" << ctx << ",idlesize=" << m_idleCtxs.size();
}
bool CIOThread::register_notifyevent() {
    auto tmpFuncPtr = boost::bind(&CIOThread::thread_handle_newconnection, _1, _2, this/*_3*/, _4);
    notify_watcher_.reset(new PipeEventWatcher(m_elthread->get_eventbase(), tmpFuncPtr));

    if ( !notify_watcher_->Init() || !notify_watcher_->AsyncWait() ) {
        LOG_ERROR << " notify watcher init failed!";
        return false;
    }
    return true;
}
bool CIOThread::register_timerevent() {
    return true;
}
Item_t* CIOThread::get_connitem() {
    if (m_newConns == NULL) {
        return NULL;
    }

    Item_t * item = NULL;
//    LOG_INFO << "get_connitem m_newConns=" << m_newConns;
//    LOG_INFO << "get_connitem m_newConns->m_connQueue.size=" << m_newConns->m_connQueue.size();

    muduo::MutexLockGuard tmp(m_newConns->m_newlock);
//    LOG_INFO << "lock beg";
    while (!m_newConns->m_connQueue.empty()) {
        item = m_newConns->m_connQueue.front();
        m_newConns->m_connQueue.pop_front();
        if (item != NULL) {
//            LOG_INFO << " get socket -ID- " << item->m_socket;
            break;
        }
    }
//    LOG_INFO << "lock end";
    return item;
}

ClientCtx* CIOThread::get_clientctx() {
    ClientCtx* ctx = nullptr;
    while (!m_idleCtxs.empty()) {
        ctx = m_idleCtxs.front();
        m_idleCtxs.pop();
        if (ctx && ctx->ctxIsInUse()) {
            LOG_ERROR << " ~~~~~~~~~ why ctx in use " << ctx;
            ctx = NULL;
            continue;
        }
        break;
    }
    if (!ctx) {
        ctx = new ClientCtx(this, P_TEST_PROCESS, PRO_BINARY);
    } else { }
    return ctx;
}

void CIOThread::thread_handle_newconnection(int fd, short which, void *arg, void *user_data) {
    CIOThread* pthis = reinterpret_cast<CIOThread*>(arg);
    if (pthis == nullptr || !user_data) {
        LOG_ERROR << " bad param " << fd;
        return;
    }
    Item_t* newItem = nullptr;
    char* puser = (char*)user_data;

    switch (puser[0]) {
        case 'c':
            newItem = pthis->get_connitem();
            if (newItem == nullptr) {
                LOG_ERROR << "newItem == nullptr";
                return;
            }
            LOG_INFO << "recv new connetction, fd=" << fd << " itemsocket=" << newItem->m_socket;
            if (!pthis->create_connection(newItem)) {
                LOG_ERROR << ", close itemsocket=" << newItem->m_socket;
                close(newItem->m_socket);
            }
            delete_object(newItem);
            break;
        default:
            LOG_ERROR << "default unknown buf..";
            return;
    }
}

bool CIOThread::create_connection(Item_t* newItem) {
    ClientCtx* newCtx = get_clientctx();
    if (!newCtx) {
        return false;
    }
    bool ret = true;
    if (!newCtx->init(newItem)) {
        ret = false;
//        activeCtxnum_--;
        delete_object(newCtx);
//        LOG_ERROR << " init ctx error, ctx=" << ctx << ",ctx activeNum=" << activeCtxnum_;
        return false;
    }
    return true;
}


void CIOThread::setPushConnection(int64_t id, ClientCtx *ctx) {
    if (id <= 0 || ctx == NULL)
        return;
    LOG_INFO << " set session id " << id << " ctx " << ctx;
    pushConnections_[id] = ctx;
}

void CIOThread::deletePushConnection(int64_t id, ClientCtx * ctx) {
    boost::unordered_map<int64_t, ClientCtx*>::iterator it = pushConnections_.find(id);
    if (it != pushConnections_.end() && it->second == ctx) {
        pushConnections_.erase(id);
    }
}
