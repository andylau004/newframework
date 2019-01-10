
#pragma once


#include <stdio.h>
#include <event.h>
#include <boost/shared_ptr.hpp>

#include <thrift/protocol/TProtocol.h>
#include <thrift/protocol/TBinaryProtocol.h>


#include "muduo/base/Mutex.h"

#include "common_stucts.hpp"
#include "sigslot.h"


using namespace apache::thrift;
using namespace std;
using namespace muduo;


typedef struct conn_ctx_s {
    short   m_flags; /**< event flags */
    SOCKET  m_sfd;
    struct  event m_event;
    struct  event m_rdEvent;
    struct  event m_wrEvent;
} conn_ctx_t;


class CFrameTransport;
class CIOThread;
class CTestProcess;

class ClientCtx : public sigslot::has_slots<>,
                  public boost::enable_shared_from_this<ClientCtx> {
private:
    uint64_t m_uIdx;
public:
    void     inline SetIdx( uint64_t uIdx ) { m_uIdx = uIdx; }
    uint64_t inline GetIdx( ) { return m_uIdx; }

public:
    /**
     *  ClientCtx 会绑定到特定的线程上，生命周期里是不会更换线程的，所以
     *  m_thread 一次的赋值就不会再改变了，直到CTX被彻底的释放
     */
    ClientCtx(CIOThread * threads, TypeProcess tp, TypeProtocol proc);
    ~ClientCtx();

    bool init(Item_t * item);

    /**
     * @brief on_closesocket 正常状态下的关闭, 即, 客户端处于正确的写或读状态
     * 这种情况下, 服务端需要判断 后台是否还有未完成的任务, 如果有延迟回收ctx, 如果没有回收ctx
     */
    void on_closesocket();
    void close_connection(bool isRecycle);
    /**
     * @brief on_clientclosesocket 非正常状态下的关闭, 即, 客户端由未完成的写状态跳转到读状态
     * 这种情况下, 服务端需要判断 后台任务所处的阶段, 根据不同的阶段做相应的处理, 还需要判断是否有其他跟本次调用无关的任务(如果有延迟回收资源)
     */
    void on_clientclosesocket();
    void on_readsocketdone();
    void on_recyclectx();
    void on_setwritestate();
    void on_setreadstate();
    void on_writesocket();

    void on_recvsomedata();

    void pushMessage(int64_t sessionID);

    void setSessionID(int64_t sessionID);
    void setClientTicket(const std::string& ticket);
    void setClientNotifyID(int64_t notifyID);

    std::string getClientTicket();
    int64_t getClientNotifyID();
    int64_t getSessionID();

//    boost::shared_ptr<protocol::TProtocol> getOutProtocol(){ return m_outputProtocol; }

    CIOThread * get_iothread();

    bool ctxIsInUse() const { return isInUse; }
    time_t get_idle_time() const {return m_idle_time;}
    inline void set_idle_time() { m_idle_time = time(NULL); }
private:
//    bool set_ctx_read();
//    bool set_ctx_write();
//    bool set_ctx_idle();
    bool set_ctx_flags(short flags);

    void set_write_idle_ctxnew();

    bool set_read_ctxnew();
    bool set_write_ctxnew();
    bool set_ctx_flags_ctxnew(short flags, struct event* pEvent, int oprflag);
    void recycle_ctx();
    static void connection_event_handler(SOCKET fd, short which, void * v);
    static void read_handler(SOCKET fd, short which, void * v);
    static void write_handler(SOCKET fd, short which, void * v);
    static ClientCtx* CheckParam(SOCKET fd, short which, void * v, const char* preFix);

//    bool set_ctx_idle_ctxnew();

public:
    void setContext(const boost::any& context)
    { context_ = context; }

    const boost::any& getContext() const
    { return context_; }

    boost::any* getMutableContext()
    { return &context_; }

private:
    boost::any      context_;

private:
    bool            isInUse;

    TypeProcess     m_typeProcess;
    TypeProtocol    m_protocol;

    int64_t     m_sessionID;
    int64_t     m_notifyID;
    time_t      m_idle_time;

    CIOThread       * m_pIoThread; /**< 依附的线程，不能在这里被删除 */
    conn_ctx_t      * m_ctx;
    CFrameTransport*  m_transport;

    CTestProcess*     m_pProcess;

//    boost::shared_ptr<protocol::TProtocol> m_inputProtocol;
//    boost::shared_ptr<protocol::TProtocol> m_outputProtocol;

//    std::string      m_ticket;
    muduo::MutexLock m_clientLock;
};
