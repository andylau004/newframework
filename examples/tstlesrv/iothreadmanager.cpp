

#include "iothreadmanager.hpp"

#include <unistd.h>
#include <iostream>

#include "muduo/base/Logging.h"


#include "eventloopthread.h"
#include "iothread.h"




CIoThreadMgr::CIoThreadMgr(int numthreads) :
    m_nthreads(numthreads),
    m_nextthreads(0) {

}

CIoThreadMgr::~CIoThreadMgr() {
    destroy();
}

bool CIoThreadMgr::start(/*TypeProcess tp, TypeProtocol proc*/) {
    int threadIndex = 1;
    int j = 0;
    while (j++ < m_nthreads) {
        CIOThread * t = new CIOThread(threadIndex/*, tp, proc*/);
        if (t->start()) {
            ++threadIndex;
            m_threads.push_back(t);
        } else {
            delete_object(t);
        }
    }
    m_nthreads = (int)m_threads.size();
    return m_nthreads > 0;
}

Item_t * CIoThreadMgr::get_item() {
    Item_t* item = new Item_t;
    return item;
}

// 这里没做安全的检查和控制
// 1. 异常 失败 检查
// 2. io线程饱和检查
bool CIoThreadMgr::dispatch_item(Item_t * item) {
    if (item == nullptr || m_nthreads <= 0) {
        return false;
    }

    int selected = m_nextthreads;
    m_nextthreads = (m_nextthreads + 1) % m_nthreads;

    CIOThread * t = m_threads[selected];
    LOG_INFO << "dispatch io_tid=" << t->get_tid() << ", socket=" << item->m_socket;
    if (t == NULL) {
        return false;
    }
    if (!t->set_item(item)) {
        LOG_ERROR << "set_item failed!!!";
        return false;
    }
    return true;
}

void CIoThreadMgr::destroy() {
    for (size_t i = 0; i < m_threads.size(); ++i) {
        CIOThread * t = m_threads[i];
        delete_object(t);
    }
}
