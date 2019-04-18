

#include "cpp11fun.h"


#include <stdio.h>
#include <string>
#include <stdint.h>

#include <stdio.h>
#include <sstream>
#include <iostream>

#include <string.h>


#include <utility>
#include <vector>



void ProcessValue(int& i)
{
    std::cout << "LValue processed: " << i << std::endl;
}

void ProcessValue(int&& i)
{
    std::cout << "RValue processed: " << i << std::endl;
}

void process_value(int& i) {
    std::cout << "LValue processed: " << i << std::endl;
}

void process_value(int&& i) {
    std::cout << "RValue processed: " << i << std::endl;
}

class Moveable {
public:
    Moveable() : i(new int(3)) {
        std::cout << "Moveable::Moveable() 构造函数 : ptr(i)=" << i << std::endl;
    }
    ~Moveable() {
        std::cout << "Moveable::~Moveable() 析构函数 ptr(i)=" << i << std::endl;
        if (i) {
            delete i;
            i = nullptr;
        }
    }
    Moveable(const Moveable& m) : i(new int(*m.i)) {
        std::cout << "Moveable::Moveable(const Moveable &) 拷贝构造函数 ptr(i)=" << i << std::endl;
    }
    Moveable(Moveable&& m) {
        this->i = m.i;
        m.i = nullptr;
        std::cout << "Moveable::Moveable(Moveable &&) 移动构造函数 ptr(i)=" << this->i << std::endl;
    }
    void Set(int x) {
        *i = x;
    }

    const int* Get() {
        return i;
    }

private:
    int* i;
};

int test_move2()
{
//    Moveable a;
//    a.Set(5);
//    const int* ptr=a.Get();
//    Moveable b(a);

//    std::cout << "a ptr=" << a.Get() << "\n";
//    std::cout << "b ptr=" << b.Get() << "\n";

    {
        Moveable a;
        a.Set(5);
        const int* ptr=a.Get();
        Moveable b(std::move(a));

        std::cout << "a ptr=" << a.Get() << "\n";
        std::cout << "b ptr=" << b.Get() << "\n";
    }
    return 1;

//    int a = 1;
//    ProcessValue(a);
//    // std::move函数可以以非常简单的方式将左值引用转换为右值引用
//    ProcessValue(std::move(a));
//    std::cout << "last a= " << a << std::endl;

//    std::string a("test string");
//    std::string b(std::move(a));
//    std::cout << "a:" << a << std::endl << "b:" << b << std::endl;

    return 1;

//    int a = 0;
//    process_value(a);
//    process_value(1);

//    std::cout<<"test std::move:\n";
//    std::string str5 = "asdf";
//    std::string &lr5 = str5;
//    std::string &&rr5 = std::move(str5);
//    rr5[0] = 'b';
//    lr5[1] = 'z';
//    std::cout<<rr5<<'\t'<<lr5<<'\t'<<str5<<std::endl;
//    std::cout<<"test std::move done.\n"<<std::endl;

//    return 0;
}



int test_move1()
{
    std::string str = "Hello";
    std::vector<std::string> v;

    // uses the push_back(const T&) overload, which means we'll incur the cost of copying str
    v.push_back(str);
    std::cout << "After copy, str is \"" << str << "\"\n";

    // uses the rvalue reference push_back(T&&) overload, which means no strings will be copied;
    // instead, the contents of str will be moved into the vector.
    // This is less expensive, but also means str might now be empty.
    v.push_back(std::move(str));
    std::cout << "After move, str is \"" << str << "\"\n";

    std::cout << "The contents of the vector are \"" << v[0] << "\", \"" << v[1] << "\"\n";

    return 0;
}







class Y {// : public boost::enable_shared_from_this<Y> {
public:
    boost::shared_ptr<Y> f() {
//        return shared_from_this();
        return boost::shared_ptr<Y>(this);
    }
    Y* getthis() {
        return this;
    }
};


void tst_shared_ptr_1() {

    boost::shared_ptr<Y> p(new Y);
    std::cout << "p count=" << p.use_count() << std::endl;

    boost::shared_ptr<Y> q = p->f();
    std::cout << "q count=" << q.use_count() << std::endl;
    std::cout << "p count=" << p.use_count() << std::endl;

}


std::string  convert(const std::wstring& wstr) {
    size_t size = wstr.size();
    if (size == 0) {
        return "";
    }
    size_t cs = size * sizeof(wchar_t) + 1;
    char c[cs];
    memset(c, '\0', cs);

    int n = std::wcstombs(c, wstr.c_str(), cs);
//    printf( "n=%d c=%s\n", n, c[0] );
    if (n > 0) {
        printf( "n=%d c=%s\n", n, std::string(c, n).c_str() );
        return std::string(c, n);
    }
    return "";
}

std::string dealExceed128Bytes( const std::string& tip ) {
    if ( tip.length() <= 128 )
        return tip;

    std::string ret_tip;

    // 截断128字节数据，且追加后缀 ...
    const int FIX_MAX_LEN = 128;
//    if (tip.length() > FIX_MAX_LEN) {
//        result.promtStKey = "newMSG";
//        return;

        printf( "tip.length() > 128, tip.length()=%d\n", tip.length() );
//        size_t tmplen = tip.length();

        wchar_t tmp_wbuffer[FIX_MAX_LEN + 1];
        ::memset( tmp_wbuffer, 0, sizeof(tmp_wbuffer) );
        size_t transferLen = std::mbstowcs(tmp_wbuffer, tip.c_str(), FIX_MAX_LEN);
//        LOG_INFO( "transferLen=" << transferLen );

        std::wstring ws_tmp = tmp_wbuffer;
//        std::wcout << "ws_tmp=" <<ws_tmp << std::endl;

        std::string slst = convert(ws_tmp);
//        char lstbuffer[ FIX_MAX_LEN*sizeof(wchar_t) + 1 ];
//        memset( lstbuffer, 0, FIX_MAX_LEN*sizeof(wchar_t) + 1 );
//        std::cout << "aaaaaaaaaa=" << FIX_MAX_LEN*sizeof(wchar_t) + 1 << std::endl;//

//        transferLen = std::wcstombs(lstbuffer, tmp_wbuffer, FIX_MAX_LEN*sizeof(wchar_t) + 1);
//        std::cout << "lst transferLen=" << transferLen << std::endl;//
//        std::cout << "lstbuffer=" << lstbuffer << std::endl;//

        if (slst.length() == tip.length()) {
            ret_tip += slst;
        } else {
            ret_tip += slst;
            ret_tip += "...";
        }

//        LOG_DEBUG( "transferLen_1=" << transferLen << "&last_tip=" << ret_tip.c_str() );
//        printf( "slst=\n", slst.c_str() );
//        printf( "ret_tip.size=%d&last_tip=%s\n", ret_tip.size(), ret_tip.c_str() );

//    }
    return ret_tip;
}









