





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

//class B{
//    public:
//        int data;
////        B(int _data):data(_data){}
//        explicit B(int _data):data(_data){}
//};
//int tst_exlic() {
//    B temp=5;
//    std::cout<<temp.data<<std::endl;
//    return 0;
//}

class Base : public std::enable_shared_from_this< Base > {
public:
    Base() { printf( "Base cst\n" ); }
    Base(int val) { val_ = val; printf( "Base val cst\n" ); }

public:
    virtual ~Base() { printf( "Base dst val_=%d\n", val_ ); }
    void showval() {printf( "val=%d\n", val_ ); }
public:
    std::shared_ptr<Base> share_this() {
//        return shared_from_this();// correct useage
        return shared_ptr<Base>(this);
    }
public:
    int val_;
};
class DeriveA : public Base {
public:
    DeriveA() { printf( "DeriveA cst\n" ); }
    virtual ~DeriveA() { printf( "DeriveA dst\n" ); }
};

//Base g_baseObj;

//const Base& get_global_base() {
//    return g_baseObj;
//}

class CMyObj;
typedef boost::shared_ptr<CMyObj> CMyObjPtr;

typedef boost::function<void (const CMyObjPtr&)> TestCallback;

class CMyObj : public boost::enable_shared_from_this<CMyObj>
{
public:
    CMyObj(TestCallback inCb, TestCallback otherCb) {
        connCb_ = inCb;
        other1Cb_ = otherCb;
    }

    void HandleClose() {

        CMyObjPtr guardThis(shared_from_this());
        LOG_INFO << "aaa guardThis usecount=" << guardThis.use_count();
        connCb_(guardThis);
        LOG_INFO << "bbb guardThis usecount=" << guardThis.use_count();
        if (other1Cb_) {
            LOG_INFO << "GOT IT";
            other1Cb_(guardThis);
        }
        LOG_INFO << "ccc guardThis usecount=" << guardThis.use_count();

    }

public:
    TestCallback connCb_;
    TestCallback other1Cb_;
};

void CallConnnCb(const CMyObjPtr& obj) {
    LOG_INFO << "CallConncb inner use_count=" << obj.use_count();
}
void OtherConnnCb(const CMyObjPtr& obj) {
    LOG_INFO << "OtherConnnCb inner use_count=" << obj.use_count();
}

void tst_share_usecount() {

    boost::shared_ptr<CMyObj> sp_obj(new CMyObj(CallConnnCb, OtherConnnCb));
    sp_obj->HandleClose();
//    sp_obj.CallConnnCb();

    return;

    std::shared_ptr< Base > sp2;
    {
        std::shared_ptr< Base > sp1 = std::shared_ptr<Base>( new Base(777) );
        LOG_INFO << "sp1 usecount=" << sp1.use_count();
        sp2 = sp1;
        LOG_INFO << "cpy sp1 usecount=" << sp1.use_count();
    }
    LOG_INFO << "sp2 usecount=" << sp2.use_count();
}


//class RowReader {
//public:
//    RowReader() { printf( "RowReader cst\n" ); }
//    ~RowReader() { printf( "RowReader dst\n" ); }
//};
//typedef void (*CallBack)(RowReader* reader);

//class LinkbaseReader : public boost::enable_shared_from_this<LinkbaseReader> {
//public:
//    LinkbaseReader() {
//        printf("LinkbaseReader::LinkbaseReader this:%p\n", this);
//    }
//    ~LinkbaseReader() {
//        printf("LinkbaseReader::~LinkbaseReader this:%p\n", this);
//    }

//public:
//    struct Context {
//        Context(const boost::shared_ptr<LinkbaseReader>& linkbase_reader_) :
//            linkbase_reader(linkbase_reader_) {}
//        boost::weak_ptr<LinkbaseReader> linkbase_reader;
//    };//Context

//    RowReader* read();
//    static void read_callback(RowReader* row_reader);
//};//LinkbaseReader
//RowReader* LinkbaseReader::read() {
//    Context* context = new Context(shared_from_this());
//    return new RowReader(context, &LinkbaseReader::read_callback);//do nothing
//}

//void LinkbaseReader::read_callback(RowReader* row_reader) {
//    LinkbaseReader::Context* context = static_cast<LinkbaseReader::Context*>(row_reader->context);
//    boost::shared_ptr<LinkbaseReader> linkbase_reader = context->linkbase_reader.lock();
//    if (linkbase_reader.get() != NULL) {
//        printf("LinkbaseReader::read_callback linkbase_reader:%p\n", linkbase_reader.get());
//    } else {
//        printf("LinkbaseReader::read_callback linkbase_reader has been deleted.\n");
//    }
//    delete context;
//    delete row_reader;
//}
//int tst_row_use() {
//    RowReader* row_reader = NULL;
//    {
//        boost::shared_ptr<LinkbaseReader> linkbase_reader(new LinkbaseReader);
//        row_reader = linkbase_reader->read();
//    }

//    //假装被回调，此时linkbase_reader已经析构
//    LinkbaseReader::read_callback(row_reader);
//    return 0;
//}

void tst_lambad_1() {
    int i_1 = 123;
    LOG_INFO << "before call " << " i_1 addr=" << &i_1;

    auto pfn_1 = [ &i_1 ]( std::string str_aaaa) {
        LOG_INFO << "i_1=" << i_1 << " i_1 addr=" << &i_1;
        LOG_INFO << "str_aaaa=" << str_aaaa;

    };

    pfn_1( std::string("bbbbbbbbbbbbbbccccccccccccccccc") );

}

void tst_vec() {
    std::vector < int > v_data;
    v_data.push_back( 123 ); v_data.push_back( 222 );
    v_data.push_back( 333 ); v_data.push_back( 444 );
    v_data.push_back( 222 );
    for ( auto it : v_data ) {
        LOG_INFO << " " << it;
    }
}
int main(int argc, char *argv[])
{
    Logger::setLogLevel(Logger::DEBUG);
    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

    tst_vec(); return 1;

    tst_share_usecount();        return 1;

    {
//        tst_row_use(); return 1;
        tst_lambad_1(); return 1;



        // 创建一个unique_ptr实例
        std::unique_ptr<Base> pInt(new Base(123456));

//        cout << *pInt;
        return 1;
    }
    {
//        Base tmpBase = get_global_base();
//        LOG_INFO << "g_baseObj addr=" << &g_baseObj;
//        LOG_INFO << "tmpBase addr=" << &tmpBase;
//        return 1;
    }

    {
//        DeriveA aaa;
        std::shared_ptr<Base> sp1(new Base);
        std::shared_ptr<Base> sp2 = sp1->share_this();
        sp1->val_ = 111111111;

        std::cout << "sp2 usecount=" << sp2.use_count() << std::endl;
        std::cout << "val=" << sp2->val_ << std::endl;

        return 1;
    }
//    tst_exlic(); return 1;

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

















