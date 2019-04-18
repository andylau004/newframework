
#pragma once


#include <stdio.h>
#include <stdint.h>


#include "muduo/base/common.h"
#include "muduo/net/Buffer.h"

#include "sigslot.h"


//#include <thrift/transport/TTransport.h>
//#include <thrift/transport/TBufferTransports.h>

//using namespace apache::thrift;

using namespace muduo::net;


class BaseTransport {
//    typedef boost::shared_ptr<transport::TMemoryBuffer> TMemoryBufferPtr;
public:
    BaseTransport();
    BaseTransport(uint32_t readBufferSize, uint32_t writeBufferSize);
//    /*explicit*/ BaseTransport(uint32_t size = 512);
    virtual ~BaseTransport();

    virtual bool init();

    virtual void work_socket(int sfd) = 0;
    virtual void read_socket(int sfd) = 0;

    virtual int32_t write_buffer(const uint8_t * buf, uint32_t len) = 0;
    virtual void write_socket(int sfd) = 0;
    virtual void reset_write_state();

    sigslot::signal0<> SignalCloseSocket;
    sigslot::signal0<> SignalClientCloseSocket;
    sigslot::signal0<> SignalReadSocketDone;
    sigslot::signal0<> SignalSetWriteState;
    sigslot::signal0<> SignalSetReadState;

    sigslot::signal0<> Signal_ClearWriteEvent;

    // 通知收到客户端数据
    sigslot::signal0<> SingalRecvSomeData;

//    TMemoryBufferPtr getInputTransport()    { return m_inputTransport; }
//    TMemoryBufferPtr getOutputTransport()   { return m_outputTransport; }

    inline std::string  GetPeerIp() {return m_peerIp;}
    inline void         SetPeerIp(const std::string& info) {m_peerIp = info;}

protected:
//    int32_t read_sock(/*uint8_t * buf, uint32_t len*/);
    int32_t read_sock_buffer_len(uint8_t * buf, uint32_t len);

    bool read_data();
    bool write_data();

protected:
    Buffer          m_in_buffer_;
    Buffer          m_outputBuffer_; // FIXME: use list<Buffer> as output buffer.

    SOCKET          m_socket;
    std::string     m_peerIp;

//    uint8_t * m_readBuffer;
//    uint8_t * m_writeBuffer; /**< 不能被删除，由TMemoryBuffer管理 */

//    uint32_t m_readBufferSize;
//    uint32_t m_writeBufferSize;
//    uint32_t m_readBufferPos;
//    uint32_t m_writeBufferPos;

//    TMemoryBufferPtr m_inputTransport;
//    TMemoryBufferPtr m_outputTransport;
};

