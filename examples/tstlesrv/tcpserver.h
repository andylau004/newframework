
#pragma once




#include <map>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>


#include <event.h>
#include <event2/util.h>

#include "muduo/base/common.h"




using namespace std;
using namespace muduo;
//using namespace muduo::net;

//using boost::shared_ptr;


class CEventLoopThread;
class CIoThreadMgr;

class CTcpServer : boost::noncopyable {

public:
    CTcpServer();
    ~CTcpServer();

public:
    bool start();
    bool init( unsigned int port, int numthreads);

private:
    bool create_and_listen_onsocket();
    bool create_and_bind_socket();
    bool listen_socket();

    bool register_events();
    void handle_event(SOCKET fd, short which);

    static void listen_handler(evutil_socket_t fd, short which, void* v);
private:
    unsigned int     m_port;
    SOCKET  m_sock_listen;

    CEventLoopThread    * m_listenThread;
    CIoThreadMgr     * m_tmanager;

    struct event m_listenEvent;

};

