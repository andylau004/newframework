


#include <iostream>

#include <sstream>





#include "tst_1.h"

#include "codeconvert.h"


//#include "poco/Bugcheck.h"
//#include "poco/Foundation.h"

//#include "poco/Alignment.h"


#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoopThreadPool.h"


#include "muduo/base/Logging.h"

#include "discardserver.h"

#include "quicksort.h"
#include "tst_signal.h"
#include "mysqlinsert.h"
#include "cpp11fun.h"



using namespace std;
using namespace muduo;
using namespace muduo::net;

extern std::string dealExceed128Bytes( const std::string& instr );

void runInThread()
{
    printf("runInThread(): pid = %d, tid = %d\n",
           getpid(), CurrentThread::tid());
}

void tst_eventloopt() {
    printf("main(): pid = %d, tid = %d\n",
           getpid(), CurrentThread::tid());

    Logger::setLogLevel(Logger::DEBUG);

    EventLoopThread loopThread;
    EventLoop *loop = loopThread.startLoop();
    // 异步调用runInThread，即将runInThread添加到loop对象所在IO线程，让该IO线程执行
//    loop->runInLoop(runInThread);
//    sleep(1);
    // runAfter内部也调用了runInLoop，所以这里也是异步调用，让该IO线程添加一个2s定时器
    loop->runAfter(2, runInThread);
    sleep(3);
    //~EventLoopThread()会调用loop_->quit();

    printf("exit main().\n");
}

void some_callback( EventLoop* inLoop ) {
//    printf( "some_cb begin\n");
//    runInThread();
//     printf( "inLoop addr: %p\n", inLoop );
//     printf( "some_cb end\n");
}
void tst_elPool_1() {
    printf("main(): pid = %d, tid = %d\n",
           getpid(), CurrentThread::tid());

    printf( "\n\n" );
//    runInThread();

    EventLoop loop;
    EventLoopThreadPool loopthreadPool( &loop, "sub reactor" );
    loopthreadPool.setThreadNum(2);
    loopthreadPool.start(some_callback);

    for ( int i = 0 ; i < 5; ++ i ) {
        EventLoop* tmploop = loopthreadPool.getNextLoop();
        tmploop->runInLoop( runInThread );
        printf( "for tmploop=%p\n", tmploop );
    }

    sleep( 5 );
    printf("exit main().\n");
}


class CTest {
public:
    int    m_iValue;
public:
    CTest() {}
    CTest( int invalue ) {
        m_iValue = invalue;
    }
    virtual ~CTest() {

    }
};

typedef boost::shared_ptr< CTest > CTestPtr;
typedef std::map<int, CTestPtr>    Map_CTest;

void tst_CTestPtr() {
    CTestPtr  newConnect( new CTest(123) );
    Map_CTest map_connections_;

    LOG_INFO << "before conn use_count=" << newConnect.use_count();
    map_connections_[ 1111 ] = newConnect;
    LOG_INFO << "after  conn use_count=" << newConnect.use_count();

    for ( Map_CTest::iterator itConn = map_connections_.begin(); itConn != map_connections_.end(); ++itConn ) {
        LOG_INFO << "key=" << itConn->first << ", Val=" << itConn->second.use_count();
    }

}


void print_arra_int( int array_nums[], int ilen ) {
    std::string strout;
    int index = 0;
    for ( index = 0; index < ilen; index ++ ) {
        char tmpbuff[ 128 ] = {0};
        sprintf(tmpbuff, "%d ", array_nums[index]);
        strout += tmpbuff;
    }
    std::cout << "strout=" << strout << std::endl;
}

void print_vect_int( const std::vector<int>& array_nums ) {
    std::string strout;
    for ( std::vector<int>::const_iterator it = array_nums.begin(); it != array_nums.end(); it ++ ) {
        char tmpbuff[ 128 ] = {0};
        sprintf(tmpbuff, "%d ", *it);
        strout += tmpbuff;
    }
    std::cout << "strout=" << strout << std::endl;
}
// 计数排序
void sort_it( std::vector<int>& array_nums ) {
    print_vect_int(array_nums);

    int count [ 3 ] = {0};
    for ( std::vector<int>::iterator it = array_nums.begin(); it != array_nums.end(); it ++ ) {
        if ( *it == 0 || *it == 1 || *it == 2 )
            count [ *it ] ++;
    }

    int slot  = 0;
    int index = 0;
    for ( index = 0; index < count[0]; ++index )
        array_nums[ slot++ ] = 0;

    for ( index = 0; index < count[1]; ++index )
        array_nums[ slot++ ] = 1;

    for ( index = 0; index < count[2]; ++index )
        array_nums[ slot++ ] = 2;

    print_vect_int(array_nums);
}
// 计数排序
void jishu_sort( ) {
    std::vector<int> array_nums;
    array_nums.push_back(0);array_nums.push_back(2);
    array_nums.push_back(1);array_nums.push_back(1);
    array_nums.push_back(2);array_nums.push_back(1);array_nums.push_back(0);
    sort_it(array_nums);
}



std::wstring tst_convert(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    int size = str.size();
    wchar_t wtmpbuffer[size+1];
    memset(wtmpbuffer, '\0', sizeof(wchar_t) * (size+1));

    std::cout << "size=" << size << std::endl;
    std::cout << "sizeof(wchar)=" << sizeof(wtmpbuffer) << std::endl;
    std::cout << "sizeof(wchar_t) * (size+1)=" << sizeof(wchar_t) * (size+1) << std::endl;

    int n = std::mbstowcs(wtmpbuffer, str.c_str(), size+1);
    if (n <= 0) {
        std::cout << "mbstowcs transfer failed!" << std::endl;
        return std::wstring();
    }

    std::wstring wstr (wtmpbuffer, n);
//    std::cout << n << " : size :  " << wstr.size() << " word:" << str << "\n";
    return wstr;
}
void tst_transfer( ) {

//    std::string str="1234567->我爱我家891011";
//    std::wstring wstr=L"1234567->我爱我家891011";
//    std::cout<<str<<endl;
//    std::wcout<<wstr<<endl;            //输出汉字失败，
//    return ;

    std::stringstream ottt;
//    ottt << "中国N国屏蔽关键词";
    ottt << "nba";

    std::string str_ttt_1 = ottt.str();

    std::wstring sret_w = tst_convert( str_ttt_1 );
    std::cout << "srclen="  << str_ttt_1.size() << ", src="  << str_ttt_1.c_str() << std::endl;
    std::wcout << "wdstlen=" << sret_w.length()  << ", wdst=" << sret_w << ", wdst.size=" << sret_w.size() << std::endl;
    std::wcout << ", wdst=" << sret_w.c_str() << std::endl;

}

void tst_left_move() {
    unsigned int ulret = 0;
    int ivalue = 2;

    std::cout << "(ivalue << 8)=" << (ivalue << 8) << ", tst1=" << (ivalue << 8) / ivalue << std::endl;
    ivalue = 10;
    std::cout << "(ivalue << 8)=" << (ivalue << 8) << ", tst1=" << (ivalue << 8) / ivalue << std::endl;

}

void tst_del_it() {
    std::vector<int>  my_container;
    for (int i = 0; i < 100; ++i) {
        my_container.push_back(i);
    }

    std::vector<int>::iterator it = my_container.begin();
    for ( ;it != my_container.end(); ) {
        if (*it % 2 == 1) {
            it = my_container.erase(it);
        }
        else {
            it++;
        }
    }
}

void tst_index_1() {
    int value = 31;

    int index = (value * 2654435769) >> 28;
    std::cout << "index=" << index << std::endl;

    value = 353;
    index = (value * 2654435769) >> 28;
    std::cout << "index=" << index << std::endl;
}

class Point
{
public:
    Point()
    {
        cout << "construction" << endl;
    }
    Point(const Point& p)
    {
        cout << "copy construction" << endl;
    }
    ~Point()
    {
        cout << "destruction" << endl;
    }
    Point(Point&& m) {
//        this->i = m.i;
//        m.i = nullptr;
//        std::cout << "Point::Point(Point &&) 移动构造函数 ptr(i)=" << this->i << std::endl;
        std::cout << "Point::Point(Point &&) 移动构造函数" << std::endl;
    }

};

void tst_vector_1() {
    {
        vector<Point> pointVec;
        Point a;
        Point b;
        pointVec.push_back(a/*std::move(a)*/);
        std::cout << "size=" << pointVec.size() << std::endl;
        pointVec.push_back(b/*std::move(b)*/);
        std::cout << "size=" << pointVec.size() << std::endl;
    }

//    cout<<pointVec.size()<<std::endl;
}
void tst_trunctor() {
    std::string stttmp = "www.baidu.com阿萨斯哦鞍山市绿卡单拉是打开拉萨的看着你呢，自行车，自行车，自行车，，自行车凯斯齐威王  圣安东尼奥四大纳斯的状况才能做出这些内存模组ceshinihaoaaa!卡单拉是打开拉萨的看着你呢，收到签发请求，调用so接口完成签发后，将json返回给请求方。存入数据库时，可以 以用户唯一id和设备类型和公钥信息，作为表示查找客户端的依据。返回的证书内容，会被so接口base64编码，java服务可解码，并存储到DB，aaaaaaaaaaaaaaaaaaaaaa";
    dealExceed128Bytes( stttmp );
}

int countChineseCharsUtf8(const std::string& content)
{
    int cnt = 0;
    for (size_t i = 0; i < content.size();) {
        char c = content[i];
        if ((c & 0x80) == 0) {
            ++i;
        } else if ((c & 0xe0) == 0xc0) {
            i += 2;
        } else if ((c & 0xf0) == 0xe0) {
            i += 3;
            ++cnt;
        } else {
            abort();
        }
    }
    return cnt;
}
void tst_count_chinesenums() {
    std::string stttmp = "www阿萨萨斯哦鞍山";
    stttmp = "www.baidu.com阿萨斯哦鞍山市绿卡单拉是打开拉萨的看着你呢，自行车，自行车，自行车，，自行车凯斯齐威王  圣安东尼奥四大纳斯的状况才能做出这些内存模组ceshinihaoaaa!卡单拉是打开拉萨的看着你呢，收到签发请求，调用so接口完成签发后，将json返回给请求方。存入数据库时，可以 以用户唯一id和设备类型和公钥信息，作为表示查找客户端的依据。返回的证书内容，会被so接口base64编码，java服务可解码，并存储到DB，aaaaaaaaaaaaaaaaaaaaaa";
    stttmp = "阿萨斯，可以以用户唯一和设备类型和公钥信息，作为表示查找客户端的依据。返回的证书内容，会被接口base64编码，服务可解码，并存储到";
    std::cout << "count=" << countChineseCharsUtf8(stttmp) << std::endl;
}


int main()
{
    Logger::setLogLevel(Logger::DEBUG);    
    std::setlocale(LC_ALL, "en_US.utf8");

    tst_Tcp_server1();    return 1;

    tst_mt_cow(); return 1;

    tst_count_chinesenums(); return 1;

//    std::string stttmp = "www.baidu.com阿萨斯哦鞍山市绿卡单拉是打开拉萨的看着你呢，";
//    std::cout << "stttmp=" <<stttmp.size() <<std::endl;

    unsigned int utss = 1;
    std::cout << "sizeof(unsigned int)=" << sizeof(unsigned int) << std::endl;
    std::cout << "sizeof(utss)=" << sizeof(utss) << std::endl;
    return 1;

    tst_shared_ptr_1(); return 1;

    tst_vector_1(); return 1;

    test_move2(); return 1;

    tst_insert_badword(); return 1;

    tst_index_1(); return 1;

    TestBitMap(); return 1;

    tst_del_it(); return 1;

    tst_left_move(); return 1;

    test_move2(); return 1;
    test_move1(); return 1;

//    tst_slot_1(); return 1;

//    C11Fun_1(); return 1;
//    tst_SharedPtr_1(); return 1;

//    tst_Top_k_1(); return 1;

    tst_heapsort_fun_2(); return 1;

    tst_heapsort_fun_1(); return 1;

    tst_quick_sort(); return 1;
    tst_quick_sort_1(); return 1;


//    tst_CTestPtr(); return 1;
    tst_transfer();

    return 1;


    tst_discard_server(); return 1;


    tst_elPool_1(); return 1;

    tst_eventloopt(); return 1;
    tst_1(); return 1;


    tst_epoll_1(); return 1;



    void* pp1 = NULL;
    std::cout << "a=" << sizeof(pp1) ;
//    cout << "Hello World!" << endl;
    return 0;
}
