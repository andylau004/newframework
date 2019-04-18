
#include "taskthreadpool.h"


#include "muduo/base/Logging.h"


CTaskThreadPool::CTaskThreadPool(int numThreads)
    : latch_(numThreads)
{
    threads_.reserve(numThreads);

}

CTaskThreadPool::~CTaskThreadPool() {

}

void CTaskThreadPool::threadFunc() {
    LOG_DEBUG << " start work";
    latch_.countDown();

    bool running = true;
    while (running)
    {
        ClientCtxPtr newtask = m_ctx_tasks.take();
    }

}

void CTaskThreadPool::start() {

    for (int i = 0; i < threads_.capacity(); ++i) {
        char name[128] = {0};
        sprintf(name, "task thread %d", i);

        // if c++99
        threads_.push_back(new muduo::Thread(
               boost::bind(&CTaskThreadPool::threadFunc, this), std::string(name)));

        // if c++11
//        threads_.emplace_back(new muduo::Thread(
//               boost::bind(&CTaskThreadPool::threadFunc, this), std::string(name)));
    }
    for_each(threads_.begin(), threads_.end(), boost::bind(&muduo::Thread::start, _1));

    // if c++11
//    for (auto& thr : threads_) {
//        thr->start();
//    }
}

/*
    * Parameter(s)  :
    * Return        :
    * Description   : 调用此接口 说明 iothread 收到一个完整的客户端报文,需要业务线程处理,并返回服务器报文
*/
void CTaskThreadPool::addtask(ClientCtxPtr newtask) {
    m_ctx_tasks.put(newtask);
}


//CTaskThreadPool g_tasks_handler(4);


