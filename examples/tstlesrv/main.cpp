





#include <iostream>

#include <sstream>


#include <stdio.h>




//#include "tst_1.h"

//#include "codeconvert.h"


#include "muduo/net/InetAddress.h"

#include "muduo/net/Channel.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"

#include "muduo/net/TcpClient.h"



#include <boost/bind.hpp>



using namespace std;
using namespace muduo;
using namespace muduo::net;


#include "tstsrv.h"

#include "tstthreadpool.h"

#include "taskthreadpool.h"


//extern  CTaskThreadPool g_tasks_handler;


class CTest {
public:
    CTest() {
        m_serverState = 123;
    }
    ~CTest() {
        m_serverState = -1;
    }
    void PrintIt() {

        m_serverState = 222;
        auto tmpCheck = [this]() -> bool {
            if (m_serverState == 123) {
                LOG_INFO << " m_serverState == 123 ";
                return false;
            } else if (m_serverState == 222) {
                LOG_INFO << " m_serverState == 222 ";
            } else {
                LOG_INFO << " m_serverState = untype ";
            }
            return true;
        };

        bool bRet = tmpCheck();
        LOG_INFO << "bRet=" << bRet;
    }
private:
    int m_serverState;
};

int tst_fun_1() {


//    CTest cct;
//    cct.PrintIt();
//    return 1;


    size_t tmplen = -1;
    if ((int)tmplen < 0) {
        LOG_INFO << "tmplen < 0, tmplen=" << tmplen;
    } else {
        LOG_INFO << "tmplen >= 0, tmplen=" << tmplen;
    }

    int i_len = (int) tmplen;
    if (i_len < 0) {
        LOG_INFO << "i_len < 0, i_len=" << i_len;
    } else {
        LOG_INFO << "i_len >= 0, i_len=" << i_len;
    }

    uint32_t u1 = 123;
    LOG_INFO << "sizeof(u1)=" << sizeof(u1);
    int32_t i2 = 4446;
    LOG_INFO << "sizeof(i2)=" << sizeof(i2);

    return 1;
}


int main(int argc, char *argv[])
{
    Logger::setLogLevel(Logger::DEBUG);
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

    tst_tp_1();
    return 1;

//    g_tasks_handler.start();
    ::sleep( 10 );
    return 1;

    tst_tp_1();
    return 1;

    return tst_libeventsrv_entry( argc, argv );

    tst_fun_1();
    return 1;

}

















