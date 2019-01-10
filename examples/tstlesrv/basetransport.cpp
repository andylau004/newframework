
#include "basetransport.h"

#include "muduo/base/common.h"
#include "muduo/base/Logging.h"
#include "muduo/net/SocketsOps.h"




BaseTransport::BaseTransport(uint32_t readBufferSize, uint32_t writeBufferSize)
    : m_socket(INVALID_SOCKET)
{
}
BaseTransport::BaseTransport()
    : m_socket(INVALID_SOCKET)
{
}
BaseTransport::~BaseTransport() {

}

bool BaseTransport::init() {
    m_socket = INVALID_SOCKET;
    return true;
}

void BaseTransport::reset_write_state() {}

// 此处接受客户端数据，可能客户端先发４字节头，也可能头+data一并发送过来
int32_t BaseTransport::read_sock(/*uint8_t * buf, uint32_t len*/) {
    if (m_socket == INVALID_SOCKET/* || buf == NULL || len == 0*/) {
        LOG_ERROR << ", sd:" << m_socket /*<< ", len:" << len*/;
        return -1;
    }
    int8_t retries = 0;
    int    got = 0;
    while (true) {
//        got = static_cast<int>(recv(m_socket, (void*)buf, len, 0));
        int savedErrno = 0;
        got = static_cast<int>(m_inputBuffer_.readFd(m_socket, &savedErrno));
        if (got < 0) {
            if (savedErrno == ERR_EAGAIN || savedErrno == ERR_EWOULDBLOCK || savedErrno == ERR_EINTR) {
                if (retries++ < 5) {
                    continue;
                }
                LOG_WARN << " exceed maxretry count, fd:" << m_socket << ", err:" << savedErrno;
            }
            // 保留 , 处理下错误
            if (savedErrno == ERR_ECONNRESET) {
                break;
            } else if (savedErrno == ERR_ENOTCONN) {
                break;
            } else if (savedErrno == ERR_ETIMEDOUT) {
                break;
            } else {
                break;
            }
        }
        break;
    }
    if (got == 0) {
        LOG_ERROR << "client: " << m_peerIp << " offline";
        SignalCloseSocket();
    }
    LOG_INFO << " fd:" << m_socket << ", got:" << got;
    return got;
}


bool BaseTransport::read_data() {
    if (m_socket == INVALID_SOCKET) {
//        LOG_ERROR << ", sd:" << m_socket << ", len:" << len;
        return false;
    }

    int8_t retrycount = 0;
    int got = 0;
    while (true) {
        int savedErrno = 0;
        ssize_t n = m_inputBuffer_.readFd(m_socket, &savedErrno);
        if (n > 0) {
            /* 如果成功读取数据，调用用户提供的可读时回调函数 */
//            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        }
        else if (n == 0) {
            /* 如果返回0，说明对端已经close连接，处理close事件，关闭tcp连接 */
//            handleClose();
        }
        else {
            errno = savedErrno;
            LOG_SYSERR << "TcpConnection::handleRead";
//            handleError();
        }
    }
    return true;
}
bool BaseTransport::write_data() {

    return true;
}








