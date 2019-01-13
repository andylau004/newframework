
#include "frametransport.h"


#include <assert.h>
#include <arpa/inet.h>

#include "muduo/base/common.h"
#include "muduo/base/Logging.h"
#include "muduo/net/SocketsOps.h"


void CFrameTransport::reset_write_state() {}

CFrameTransport::CFrameTransport()
    : m_socket_state(SOCKET_RECV_FRAMING),
      m_appState(APP_INIT),
      m_serverState(SERVER_READ),
      m_readWant(0),
      m_rd_sock_State(SOCKET_RECV_FRAMING)

{

}
CFrameTransport::~CFrameTransport() {
//    delete_object(m_framing);
}

bool CFrameTransport::init() {

//    if (m_framing == nullptr) {
//        m_framing = new (std::nothrow) Framing;
//        if (m_framing == nullptr) {
//            LOG_ERROR << " new m_framing error ";
//            return false;
//        }
//    }
    m_rd_sock_State = SOCKET_RECV_FRAMING;

//    m_socket_state = SOCKET_RECV_FRAMING;
//    m_appState    = APP_INIT;
//    m_serverState = SERVER_READ;
    m_readWant    = 0;
    return true;
}

void CFrameTransport::write_socket(int cfd) {
    if (m_socket != cfd) {
        SignalCloseSocket();
        return;
    }
    wr_transition();
}
// 此接口 -- 发送outbuffer对象内 append后 未发送完数据
void CFrameTransport::wr_transition() {
    LOG_INFO << " found some data remain in outbuffer, len=" << m_outputBuffer_.readableBytes();
    size_t n = sockets::write(m_socket,
                              m_outputBuffer_.peek(),
                              m_outputBuffer_.readableBytes());
    if ((int)n > 0) {
        m_outputBuffer_.retrieve(n);// 回收 已经发送字节数 buffer缓冲区
        if (m_outputBuffer_.readableBytes() == 0) {
            // 数据 全部发完,关闭写事件
            Signal_ClearWriteEvent();
            LOG_INFO << " send data len, n=" << n << " clear write event";
        } else {// 没有发完,先不关闭写事件,等待再次触发
        }
    } else {
        // 出现问题,关闭套接字
        LOG_ERROR << "Connection fd = " << m_socket << " is down, no more writing";
        SignalCloseSocket();
    }
}
void CFrameTransport::rd_transition() {
    switch(m_rd_app_state) {
        case APP_READ_FRAME_SIZE:
            if (m_readWant <= 0) {
                SignalCloseSocket();
                return;
            }
            m_rd_sock_State = SOCKET_RECV;
            m_rd_app_state  = APP_READ_REQUEST;
            break;
            // 只有读取数据包的长度 >= 帧头内长度值，才会返回true，然后进入下一步逻辑，也就是APP_READ_REQUEST
            // 解析body
        case APP_READ_REQUEST:
//            m_rd_app_state = APP_WAIT_TASK;
//            m_rd_srv_State = SERVER_WRITE;
            // 通知上层应用处理用户数据包，应用层处理完成后，触发写操作
            SignalReadSocketDone();

            m_rd_sock_State = SOCKET_RECV_FRAMING;
            m_rd_app_state  = APP_READ_FRAME_SIZE;
            m_readWant      = 0;

//            m_socket_state = SOCKET_RECV_FRAMING;
////            m_appState    = APP_INIT;
//            m_serverState = SERVER_READ;
//            m_readWant    = 0;

            /* 这个逻辑再考虑下实现
             * 先考虑，上层应用，拷贝数据给outbuffer_，使用outbuffer写给客户端
            */
            // 一个完整数据包读取完成后，状态重新重置
            // 再等待读取下一个用户数据包
//            init();
            break;
        case APP_WAIT_TASK:// 开始返回数据
            m_rd_sock_State = SOCKET_SEND;
            m_rd_app_state/*m_appState*/ = APP_SEND_RESULT;
            // 注册可写事件
            SignalSetWriteState();
            break;
    }
}
void CFrameTransport::read_socket(int cfd) {

    {
        if (m_socket == INVALID_SOCKET || m_rd_app_state == APP_INIT) {
            m_socket = cfd;
        } else {
            if (m_socket != cfd) {
                LOG_ERROR << " m_socket: " << m_socket << " != cfd: " << cfd << " return";
                SignalCloseSocket();
                return;
            }
        }
        //    auto funcCheckState = [this]() -> bool {
        //        if (m_rd_srv_State == SERVER_WRITE) {
        //            LOG_ERROR << ", client does not follow the rules";
        //            SignalClientCloseSocket();
        //            return false;
        //        }
        //        return true;
        //    };
    }

    ////////////////////////////////////////////////////////////////////////////
//    read_framing();
    ////////////////////////////////////////////////////////////////////////////
    switch (m_rd_sock_State) {
    case SOCKET_RECV_FRAMING:
//        if (!funcCheckState()) return;
        LOG_INFO << ", RECV_FRAMING cfd: " << cfd;
        if (read_framing()) {
            rd_transition();
        }
        break;
    case SOCKET_RECV:
//        if (!funcCheckState()) return;
        LOG_INFO << ", RECV_DATA cfd: " << cfd;
        if (read_data()) {// 只有数据读取完成 返回true后，才会触发下一步，提交应用层逻辑
            rd_transition();
        }
        break;
    default:
        LOG_ERROR << ", unknown state,  DEFAULT sd " << cfd;
        SignalCloseSocket();
        break;
    }
}
void CFrameTransport::work_socket(int cfd) {
//    if (m_socket == INVALID_SOCKET || m_appState == APP_INIT) {
//        m_socket = cfd;
//    } else {
//        if (m_socket != cfd) {
//            LOG_ERROR << " m_socket: " << m_socket << " != cfd: " << cfd << " return";
//            SignalCloseSocket();
//            return;
//        }
//    }

//    auto funcCheckState = [this]() -> bool {
//        if (m_serverState == SERVER_WRITE) {
//            LOG_ERROR << ", client does not follow the rules";
//            SignalClientCloseSocket();
//            return false;
//        }
//        return true;
//    };

//    switch (m_socket_state) {
//    case SOCKET_RECV_FRAMING:
//        if (!funcCheckState()) return;
//        LOG_INFO << ", RECV_FRAMING cfd: " << cfd;
//        if (read_framing()) {
//            rd_transition();
//        }
//        break;
//    case SOCKET_RECV:
//        if (!funcCheckState()) return;
//        LOG_INFO << ", RECV cfd: " << cfd;
//        if (read_data()) {
//            rd_transition();
//        }
//        break;
//    case SOCKET_SEND:
//        LOG_INFO << ", SEND cfd: " << cfd;
//        if (write_data()) {
//            wr_transition();
//        }
//        break;
//    default:
//        LOG_ERROR << ", unknown state,  DEFAULT sd " << cfd;
//        SignalCloseSocket();
//        break;
//    }
}
// for test
std::string CFrameTransport::GetMsg() {
    int datalen = m_in_buffer_.peekInt32_FromReserve/*peekInt32*/();
    LOG_DEBUG << " peekInt32_FromReserve datalen=" << datalen;

//    m_in_buffer_.retrieve( sizeof(int32_t) );
    return m_in_buffer_.retrieveAsString( datalen );
}
int32_t CFrameTransport::GetRemainLen() {
    return m_in_buffer_.readableBytes();
}
bool CFrameTransport::read_framing() {
//    m_in_buffer_.ensureWritableBytes( sizeof(int32_t) );
    int32_t readlen = sizeof(int32_t);

    // 数据长度,保存在预留字段8字节的前4字节
    int32_t recvBytes = read_sock_buffer_len( (uint8_t*)m_in_buffer_.begin/*beginWrite*/(), readlen );
    LOG_DEBUG << "recvBytes=" << recvBytes;
    if (recvBytes <= 0) {
        SignalCloseSocket();
        LOG_ERROR << "read_framing recvBytes == 0 some error happened!!!";
        return false;
    }
//    m_in_buffer_.hasWritten( readlen );

    m_readWant = m_in_buffer_.peekInt32_FromReserve/*peekInt32*/();
    if (m_readWant <= 0) {
        SignalCloseSocket();
        LOG_ERROR << " readwant <= 0 fatal error, val=" << m_readWant;
        return false;
    }
//    m_in_buffer_.retrieve( sizeof(int32_t) );
//    LOG_DEBUG << "m_readWant=" << m_readWant;
//    LOG_INFO << "readwhat=" << m_readWant << ", peek=" << m_in_buffer_.peek();

    m_rd_app_state = APP_READ_FRAME_SIZE;
    if (m_in_buffer_.readableBytes() >= m_readWant) {
        LOG_DEBUG << " unreachable inbuffer readablebytes: " << m_in_buffer_.readableBytes() << " >= readWant: " << m_readWant;
        SignalReadSocketDone();
//        m_socket_state = SOCKET_RECV_FRAMING;
//        m_appState    = APP_INIT;
//        m_serverState = SERVER_READ;
//        m_readWant    = 0;
    } else {//
//        LOG_DEBUG << "inbuffer readablebytes:" << m_in_buffer_.readableBytes() << " < readWant:" << m_readWant;
    }
    return true;
}
bool CFrameTransport::read_data() {
    if (m_readWant <= 0) {
        SignalCloseSocket();
        LOG_ERROR << "bad readwant val, maybe val no yet reset!!!";
        return false;
    }
    m_in_buffer_.ensureWritableBytes( m_readWant );

    int32_t shouldreadbytes = m_readWant - m_in_buffer_.readableBytes();
//    if (shouldreadbytes > 0) {
//        // 如果是第二次执行到此函数 -- 说明上次,读取数据body,没有一次性读取完成,需要再次触发,读取剩余数据body
//        LOG_DEBUG << "some thing!!! readWhat=" << m_readWant << ", shouldreadbytes=" << shouldreadbytes;
//    }

    int32_t recvBytes = read_sock_buffer_len( (uint8_t*)m_in_buffer_.beginWrite(), shouldreadbytes );
//    LOG_DEBUG << "shouldreadbytes=" << shouldreadbytes << " recvBytes=" << recvBytes << " readWhat=" << m_readWant;
    if (recvBytes <= 0) {
        SignalCloseSocket();
        LOG_ERROR << "read_data recvBytes == 0 some error happened!!! recvBytes=" << recvBytes;
        return false;
    }
    m_in_buffer_.hasWritten( recvBytes );

    size_t lenRdable = m_in_buffer_.readableBytes();
    if (lenRdable < m_readWant) {// 如果可读数据长度，小于帧头中长度，需要继续读，保证用户发送内容，被完整读取
        LOG_INFO << "need look! lenRdable:" << lenRdable << " < readWant:" << m_readWant << " need read more";
//        irecv = read_sock();
        return false;
    } else {// 只有读取数据包的长度 >= 帧头内长度值，再返回true，然后进入下一步逻辑
        LOG_INFO << "lenRdable:" << lenRdable << " >= m_readWant:" << m_readWant;
        return true;
    }
}
// 提供给上层应用，发送数据.
//              未完成发送的数据，append到outbuffer内，并注册写事件
// 返回  == 0 发送完成
// 返回  >  0 还未完成发送，需要注册写事件
// 返回  <  0 异常
int32_t CFrameTransport::write_buffer(const uint8_t * buf, uint32_t len) {
//    const void* data = m_outputBuffer_.peek();
//    size_t      len  = m_outputBuffer_.readableBytes();

    ssize_t nwrote     = 0;
    size_t  remaining  = len;
    bool    faultError = false;

    nwrote = sockets::write(m_socket, buf, len);
    if ((int)nwrote >= 0) {
        remaining = len - nwrote;
//        if (remaining == 0 && writeCompleteCallback_) {
//            loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
//        }
        if (remaining == 0) {
            return 0;
        } else {
            LOG_ERROR << " write to client:" << GetPeerIp()
                      << ", not complete alllen=" << len << " remaining=" << remaining;
        }
    } else {// nwrote < 0
        nwrote = 0;
        if (errno != EWOULDBLOCK) {
            LOG_SYSERR << "write_buffer write failed, errno=" << errno;
            if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
            {
                faultError = true;
            }
        }
    }

    assert(remaining <= len);
    if (!faultError && remaining > 0) {
        // append剩余未发完的数据
        m_outputBuffer_.append(/*static_cast<const char*>*/(const void*)(buf) + nwrote, remaining);
        // 注册写事件
        SignalSetWriteState();
        return remaining;
//        if (!channel_->isWriting()) {
//          channel_->enableWriting();
//        }
    }
    return remaining;
}

// 发送剩余未完成数据，这些数据已经append到 outbuffer 内
bool CFrameTransport::write_data() {

    return true;
}








