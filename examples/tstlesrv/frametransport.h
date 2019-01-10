
#pragma once


#include <stdio.h>
#include <stdint.h>

//#include <thrift/transport/TTransport.h>
//#include <thrift/transport/TBufferTransports.h>

//using namespace apache::thrift;

#include "basetransport.h"



enum SocketState {
    SOCKET_RECV_FRAMING,
    SOCKET_RECV,
    SOCKET_SEND
};

enum AppState {
    APP_INIT,
    APP_READ_FRAME_SIZE,
    APP_READ_REQUEST,
    APP_WAIT_TASK,
    APP_SEND_RESULT,
    APP_CLOSE_CONNECTION
};

enum ServerState {
    SERVER_READ,
    SERVER_WRITE
};



union Framing {
    uint8_t buf[sizeof(uint32_t)];
    uint32_t size;
};

#define FRAME_SIZE sizeof(uint32_t)


class CFrameTransport : public BaseTransport
{
public:
    CFrameTransport();
    virtual ~CFrameTransport();
public:
    virtual bool init();

    virtual void work_socket(int cfd);
    virtual void read_socket(int cfd);

    virtual void write_socket(int cfd);
    virtual void reset_write_state();

    std::string GetMsg();

    virtual int32_t     write_buffer(const uint8_t * buf, uint32_t len);

private:
    bool read_framing();
    bool read_data();
    bool write_data();

    void rd_transition();
    void wr_transition();

public:
//    Framing*        m_framing;
    int32_t         m_readWant;


    SocketState     m_rd_sock_State;
    AppState        m_rd_app_state;
    ServerState     m_rd_srv_State;

    SocketState     m_socket_state;
    AppState        m_appState;
    ServerState     m_serverState;


};
