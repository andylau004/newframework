
#include "clientctx.hpp"


#include <assert.h>
#include <unistd.h>
#include <iostream>

//#include "threadex.hpp"
//#include "iothread.hpp"
//#include "frametransport.hpp"



#include "muduo/base/common.h"
#include "muduo/base/Logging.h"

#include "eventloopthread.h"

#include "iothread.h"
#include "frametransport.h"

#define read_flag  0
#define write_flag 1


class CTestProcess {
public:
    CTestProcess(BaseTransport* pTransport) {
        m_pTransport = pTransport;
    }
    ~CTestProcess() {

    }
public:
    bool init() {
        return true;
    }
    void EchoIt( const std::string& onmsg ) {
        int remainBytes = m_pTransport->write_buffer( (const uint8_t *)onmsg.data(), onmsg.size() );
        if (remainBytes > 0) {
            LOG_INFO << "send to client, remainBytes>0, remainBytes=" << remainBytes;
        } else if (remainBytes < 0) {
            LOG_INFO << "send to client, remainBytes<0, remainBytes=" << remainBytes;
        }
//        LOG_INFO << "send to client, remainBytes=" << remainBytes;
//        if (remainBytes == 0) {
//        } else if (remainBytes > 0) {//
//            Signal_Write_Response();
//        } else { // < 0
//        }
    }
private:
public:
//    sigslot::signal0<> Signal_Write_Response;
//    sigslot::signal0<> SignalCloseSocket;
//    sigslot::signal0<> SignalRecycleCtx;

private:
    BaseTransport* m_pTransport;
};

ClientCtx::ClientCtx(CIOThread * inIoThread, TypeProcess tp, TypeProtocol pro) :
    m_pProcess(nullptr),
    m_ctx(nullptr),
    isInUse(false),
    m_typeProcess(tp),
    m_protocol(pro),
    m_sessionID(0),
    m_notifyID(0),
    m_idle_time(0),
    m_pIoThread(inIoThread),
    m_transport(nullptr),
//    m_processor(NULL),
//    m_ticket(""),
    m_uIdx(0) {

}

ClientCtx::~ClientCtx() {
//    LOG_INFO << "clientctx destruction, this=" << this;
    delete_object(m_ctx);
    delete_object(m_transport);
//    delete_object(m_processor);
}

/// 暂时不处理process不一致的状况, 保证process一定是一致的
bool ClientCtx::init(Item_t * item) {
    if (item == NULL || m_typeProcess == P_UNKNOWN) {
        LOG_ERROR << " item or type is error" << m_typeProcess;
        return false;
    }
    isInUse     = true;
    m_sessionID = 0;
    m_notifyID  = 0;
//    m_ticket = "";

    if (m_ctx == NULL) {
        m_ctx = new (std::nothrow) conn_ctx_t;
        if (m_ctx == NULL) {
            LOG_ERROR << " new ctx error";
            return false;
        }
    }
    m_ctx->m_sfd   = item->m_socket;
    m_ctx->m_flags = 0;

    if (m_transport == nullptr) {
        m_transport = new (std::nothrow) CFrameTransport;
        if (m_transport == nullptr) {
            LOG_ERROR << " can't new transport ";
            return false;
        }
        m_transport->SetPeerIp(item->m_type_ip + "-" + item->m_ip + ":" + convert<std::string>(item->m_port));

        m_transport->SignalCloseSocket.connect(this, &ClientCtx::on_closesocket);
//        m_transport->SignalClientCloseSocket.connect(this, &ClientCtx::on_clientclosesocket);
        m_transport->SignalReadSocketDone.connect(this, &ClientCtx::on_readsocketdone);

        m_transport->SignalSetWriteState.connect(this, &ClientCtx::on_setwritestate);
        m_transport->SignalSetReadState.connect(this, &ClientCtx::on_setreadstate);

//        m_transport->SingalRecvSomeData.connect(this, &ClientCtx::on_recvsomedata);
        m_transport->Signal_ClearWriteEvent.connect(this, &ClientCtx::set_write_idle_ctxnew);
    }
    if (!m_transport->init()) {
        LOG_ERROR << " bad transport init " << m_ctx->m_sfd;
        return false;
    }
//    m_inputProtocol  = m_thread->getInputProtocolFactory()->getProtocol(m_transport->getInputTransport());
//    m_outputProtocol = m_thread->getOutputProtocolFactory()->getProtocol(m_transport->getOutputTransport());

    if (m_pProcess == nullptr) {
        m_pProcess = new CTestProcess(m_transport);
        if (m_pProcess == nullptr) {
            LOG_ERROR << " can't new processor type: testprocess";
            return false;
        }
//        m_pProcess->Signal_Write_Response.connect(this, &ClientCtx::on_writesocket);
//        m_pProcess->SignalCloseSocket.connect(this, &ClientCtx::on_closesocket);
//        m_pProcess->SignalRecycleCtx.connect(this, &ClientCtx::on_recyclectx);
    }
    if (!m_pProcess->init()) {
        LOG_ERROR << " can't init process ";
        return false;
    }
    LOG_INFO << " new init fd " << m_ctx->m_sfd
             << " event " << &m_ctx->m_event
             << " type " << m_typeProcess
             << " protocol " << m_protocol
             << " ctx " << this;
    return set_read_ctxnew();
}

void ClientCtx::on_closesocket() {
    //LOG4CPLUS_DEBUG(gLogger, __FUNCTION__ << ", close socket " << m_ctx->m_sfd << "," << (int)isRecycle);
    close_connection(true);
}

void ClientCtx::on_clientclosesocket() {
    bool isRecycle = true;
//    if (m_processor) {
////        isRecycle = m_processor->close_apservice();
//    } else {
//        LOG_INFO << "m_processor is NULL!";
//    }
    //LOG4CPLUS_DEBUG(gLogger, __FUNCTION__ << ", close socket " << m_ctx->m_sfd << "," << (int)isRecycle);
    close_connection(isRecycle);
}

void ClientCtx::on_recyclectx() {
    recycle_ctx();
}

// 一个完整客户端报文接收完成，开始处理客户端数据包
void ClientCtx::on_readsocketdone() {
    LOG_INFO << ", finish to read socket, will process, --sd start:" << m_ctx->m_sfd << " ctx " << this;
    set_read_ctxnew();

    // -----------------------------------------------------------------------
    // 测试------------业务处理逻辑
    std::string oneMsg = m_transport->GetMsg();
    if (oneMsg.size() == 0) {
        LOG_ERROR << "some error! oneMsg.size():" << oneMsg.size()/* << " != m_transport->m_readWant: " << m_transport->m_readWant*/;
    } else {
        LOG_INFO << "from client: " << m_transport->GetPeerIp()
                 << ", msglen=" << oneMsg.size() << ", msg=" << oneMsg
                 << ", remainlen=" << m_transport->GetRemainLen();
        // echo返回
        m_pProcess->EchoIt(oneMsg);
    }

    // -----------------------------------------------------------------------

//    bool ret = false;
//    try {
//        ret = m_processor->process(m_inputProtocol, m_outputProtocol, this);
//    } catch(std::exception& e) {
//        ret = false;
//        LOG_ERROR << " bad process " << m_ctx->m_sfd << ":" << e.what();
//    } catch(...) {
//        ret = false;
//        LOG_ERROR << " bad process " << m_ctx->m_sfd;
//    }
//    //LOG4CPLUS_DEBUG(gLogger, __FUNCTION__ << ", process call end, ret:" << (int)ret << ", --sd end:" << m_ctx->m_sfd);
//    if (!ret) {
//        LOG_ERROR << ", process call error " << m_ctx->m_sfd;
//        on_closesocket();
//    }
}

// 设置event可写
void ClientCtx::on_setwritestate() {
    if (set_write_ctxnew()) {
        //LOG4CPLUS_DEBUG(gLogger, __FUNCTION__ << ", good callback sd:" << m_ctx->m_sfd);
    } else {
        LOG_ERROR << ", @@@ bad callback sd:" << m_ctx->m_sfd;
        on_closesocket();
    }
}

void ClientCtx::on_setreadstate() {
    set_read_ctxnew();
}

// 收到数据刷新时间轮
void ClientCtx::on_recvsomedata() {
//    m_thread->AddOneCtx(this/*thisPtr*/);
}

void ClientCtx::on_writesocket() {
//    LOG_INFO << ", sd:" << m_ctx->m_sfd << " ctx " << this;
    m_transport->write_socket(m_ctx->m_sfd);
}

void ClientCtx::pushMessage(int64_t sessionID) {
    if (sessionID <= 0 || sessionID != m_sessionID) {
        return;
    }
    LOG_INFO << " get msg session id " << sessionID;
//    if (m_processor) {
//        bool ret = false;
//        m_transport->getOutputTransport()->resetBuffer();
//        m_transport->getOutputTransport()->getWritePtr(4);
//        m_transport->getOutputTransport()->wroteBytes(4);

//        m_transport->reset_write_state();
//        ret = m_processor->process(m_inputProtocol, m_outputProtocol, this, m_sessionID);
//        if (!ret) {
//            LOG_ERROR << " can't process " << sessionID;
//            on_closesocket();
//        }
//    } else {
//        LOG_ERROR << " processor is nil " << sessionID;
//    }
}

void ClientCtx::setSessionID(int64_t sessionID) {
    if (sessionID <= 0) return;

    muduo::MutexLockGuard lock(m_clientLock);
    m_sessionID = sessionID;
    if (m_sessionID > 0) {
//        m_thread->setPushConnection(m_sessionID, this);
    }
}

void ClientCtx::setClientTicket(const std::string &ticket) {
    muduo::MutexLockGuard lock(m_clientLock);
//    m_ticket = ticket;
}

void ClientCtx::setClientNotifyID(int64_t notifyID) {
    muduo::MutexLockGuard lock(m_clientLock);
    m_notifyID = notifyID;
}

int64_t ClientCtx::getSessionID() {
    int64_t id = 0;
    muduo::MutexLockGuard lock(m_clientLock);
    id = m_sessionID;
    return id;
}

int64_t ClientCtx::getClientNotifyID() {
    int64_t id = 0;
    muduo::MutexLockGuard lock(m_clientLock);
    id = m_notifyID;
    return id;
}

std::string ClientCtx::getClientTicket() {
    std::string ticket;
    muduo::MutexLockGuard lock(m_clientLock);
//    ticket = m_ticket;
    return ticket;
}

CIOThread * ClientCtx::get_iothread() {
    return m_pIoThread;
}

//bool ClientCtx::set_ctx_read() {
//    return set_ctx_flags(EV_PERSIST | EV_READ);
//}

//bool ClientCtx::set_ctx_write() {
//    return set_ctx_flags(EV_PERSIST | EV_WRITE);
//}

//bool ClientCtx::set_ctx_idle() {
//    return set_ctx_flags(0);
//}

void ClientCtx::set_write_idle_ctxnew() {
//    set_ctx_flags_ctxnew(0, &(m_ctx->m_wrEvent), write_flag);
}

bool ClientCtx::set_read_ctxnew() {
    LOG_INFO << "set_read_ctxnew --------read state";
    return set_ctx_flags_ctxnew(EV_PERSIST | EV_READ, &(m_ctx->m_rdEvent), read_flag);
}
bool ClientCtx::set_write_ctxnew() {
    LOG_INFO << "set_write_ctxnew --------write state";
    return set_ctx_flags_ctxnew(EV_PERSIST | EV_WRITE, &(m_ctx->m_wrEvent), write_flag);
}
bool ClientCtx::set_ctx_flags_ctxnew(short flags, struct event* pEvent, int oprflag) {
    if (m_ctx == nullptr) {
        LOG_ERROR << " m_ctx error";
        return false;
    }

    LOG_DEBUG << " old " << m_ctx->m_flags << " new " << flags << " event " << pEvent/*&m_ctx->m_event*/;
    if (m_ctx->m_flags == flags) {
        return true;
    }

    CEventLoopThread* t = m_pIoThread->get_evlthread();
    if (t == nullptr) { return false; }

    if (m_ctx->m_flags != 0) {
        if (event_del(pEvent/*&m_ctx->m_event*/) == -1) {
            LOG_ERROR << " event_del error";
            return false;
        }
    }
    m_ctx->m_flags = flags;
    if (!m_ctx->m_flags) {
        LOG_INFO << " no pending event ";
        return true;
    }

    struct event_base* pmainbase = t->get_eventbase();
    event_callback_fn  pfnCb   = nullptr;

    if (oprflag == read_flag) {
        pfnCb = ClientCtx::read_handler;
    } else if (oprflag == write_flag) {
        pfnCb = ClientCtx::write_handler;
    }

    ::event_assign(pEvent, pmainbase, m_ctx->m_sfd, m_ctx->m_flags, pfnCb, this);
    ::event_base_set(t->get_eventbase(), pEvent);
    if (::event_add(pEvent, nullptr) == -1) {
        LOG_ERROR << " event add error!!!";
        return false;
    }
    return true;
}

//bool ClientCtx::set_ctx_flags(short flags) {
//    if (m_ctx == NULL) {
//        return false;
//    }

//    LOG4CPLUS_DEBUG(gLogger, __FUNCTION__ << " old flag " << m_ctx->m_flags
//                    << " new flag " << flags << " fd " << m_ctx->m_sfd);

//    if (m_ctx->m_flags == flags) {
//        return true;
//    }

//    Thread * t = m_thread->get_thread();
//    if (t == NULL) {
//        return false;
//    }
//    if (m_ctx->m_flags != 0) {
//        m_ctx->m_flags = t->set_eventcallback(&m_ctx->m_event, m_ctx->m_sfd,
//                                              flags, connection_event_handler, this, true);
//    } else {
//        m_ctx->m_flags = t->set_eventcallback(&m_ctx->m_event, m_ctx->m_sfd,
//                                              flags, connection_event_handler, this, false);
//    }

//    return m_ctx->m_flags == flags;
//    return true;
//}

void ClientCtx::close_connection(bool isRecycle) {
    //set_ctx_idle();
//    if (event_del(&m_ctx->m_event) == -1) {
//        LOG_ERROR << " event_del failed!!!";
//    }
    if (event_del(&m_ctx->m_rdEvent) == -1) {
        LOG_ERROR << " event_del m_rdEvent failed!!!";
    }
    if (event_del(&m_ctx->m_wrEvent) == -1) {
        LOG_ERROR << " event_del m_wrEvent failed!!!";
    }

    LOG_INFO << ", close socket " << m_ctx->m_sfd << "," << (int)isRecycle;
    if (m_ctx->m_sfd != INVALID_SOCKET) {
        shutdown(m_ctx->m_sfd, SHUT_RDWR);
        close(m_ctx->m_sfd);
        m_ctx->m_sfd = INVALID_SOCKET;
    }
    if (m_sessionID > 0) {
        LOG_INFO << " return sid " << m_sessionID;
        m_pIoThread->deletePushConnection(m_sessionID, this);
//        _sm.on_clt_disconnect(m_sessionID);
    }

    setSessionID(0);
    setClientNotifyID(0);
    setClientTicket("");

    if (isRecycle) {
        isInUse = false;
        m_pIoThread->return_clientctx(this);
    }
    LOG_INFO << ", is return ctx=" << this;
}

void ClientCtx::recycle_ctx() {
    //LOG4CPLUS_DEBUG(gLogger, __FUNCTION__ << ", recycle socket " << m_ctx->m_sfd);
    isInUse = false;
    m_pIoThread->return_clientctx(this);
}

ClientCtx* ClientCtx::CheckParam(SOCKET fd, short which, void * v, const char* preFix) {
    ClientCtx* ctx = reinterpret_cast<ClientCtx*>(v);
    if (ctx == nullptr) {
        LOG_ERROR << ", ctx is null";
        return nullptr;
    }
    if (fd != ctx->m_ctx->m_sfd) {
        LOG_ERROR << preFix << " bad fd " << fd << " old " << ctx->m_ctx->m_sfd << " " << ctx
                  << " which " << which << " event " << &ctx->m_ctx->m_event;
        return nullptr;
    }
    return ctx;
}

void ClientCtx::read_handler(SOCKET fd, short which, void * v) {
    ClientCtx* ctx = CheckParam(fd, which, v, "read_handler");
    if (!ctx) {
        return;
    }
    ctx->m_transport->read_socket(fd);
}

void ClientCtx::write_handler(SOCKET fd, short which, void * v) {
    ClientCtx* ctx = CheckParam(fd, which, v, "write_handler");
    if (!ctx) {
        return;
    }
    ctx->m_transport->write_socket(fd);
}

void ClientCtx::connection_event_handler(SOCKET fd, short which, void * v) {
//    ClientCtx* ctx = reinterpret_cast<ClientCtx*>(v);
//    if (ctx == nullptr) {
//        LOG_ERROR << ", socket " << fd;
//        return;
//    }
//    if (fd != ctx->m_ctx->m_sfd) {
//        LOG_ERROR << " bad fd " << fd << " old " << ctx->m_ctx->m_sfd << " " << ctx
//                  << " which " << which << " event " << &ctx->m_ctx->m_event;
//        return;
//    }
//    ctx->m_transport->work_socket(fd);
}
