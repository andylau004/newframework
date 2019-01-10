

#pragma once


#include <stdio.h>
#include <vector>

#include "muduo/base/common.h"
#include "common_stucts.hpp"



class CEventLoopThread;
class CIOThread;

class CIoThreadMgr {
public:
    CIoThreadMgr(int numthreads = 4);
    ~CIoThreadMgr();

    bool start(/*TypeProcess tp, TypeProtocol proc*/);
    Item_t * get_item();
    bool dispatch_item(Item_t * item);
private:
    void destroy();
private:
    int m_nthreads;
    int m_nextthreads;
    std::vector<CIOThread*> m_threads;
};
