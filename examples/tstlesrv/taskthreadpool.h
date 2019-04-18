
#pragma once



//#include <functional>
#include <memory>

#include <set>
#include <queue>


#include <boost/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>


#include "muduo/base/BlockingQueue.h"
#include "muduo/base/ThreadPool.h"

#include "concurrentqueue.h"




class ClientCtx;

typedef std::shared_ptr<ClientCtx> ClientCtxPtr;

class CTaskThreadPool {
public:
    CTaskThreadPool(int numThreads);
    virtual ~CTaskThreadPool();

public:
    void start();

public:
    void addtask(ClientCtxPtr newtask);

public:

private:
    void threadFunc();


private:

    muduo::CountDownLatch latch_;
    boost::ptr_vector<muduo::Thread> threads_;
//    std::vector< std::unique_ptr< muduo::Thread > > threads_;

    muduo::ThreadPool m_tp;


    muduo::BlockingQueue< ClientCtxPtr > m_ctx_tasks;

//    std::set< std::shared_ptr< ClientCtx > > m_setTask;

};


