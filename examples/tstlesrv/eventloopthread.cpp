

#include "eventloopthread.h"

#include "muduo/base/common.h"


#include "muduo/base/Logging.h"



CEventLoopThread::CEventLoopThread(int tId)
    : m_status(false), m_id(tId),
      m_isrun(false), m_eventBase(nullptr) {
    init();
}

CEventLoopThread::~CEventLoopThread() {
    destroy();
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);
}

int CEventLoopThread::start(bool isasync) {
    int iret = create_thread();
    if (iret != 0) return -1;

    pthread_mutex_lock(&m_mutex);
    while (!m_isrun) {
        pthread_cond_wait(&m_cond, &m_mutex);
    }
    pthread_mutex_unlock(&m_mutex);
    if (isasync) {
        return 0;
    }
    join();
    return SUCCESS;
}
void CEventLoopThread::init() {
    pthread_mutex_init(&m_mutex, nullptr);
    pthread_cond_init(&m_cond, nullptr);
    if (m_eventBase == nullptr) {
        m_eventBase = event_base_new/*event_init*/();
    }
}
void CEventLoopThread::init_eventbase() {}
void CEventLoopThread::destroy() {
    if (m_status) {
        pthread_cancel(m_tid);
        pthread_join(m_tid, nullptr);
    }
    if (m_eventBase) {
        event_base_free(m_eventBase);
        m_eventBase = nullptr;
    }
}

int CEventLoopThread::create_thread() {
    if (m_eventBase == nullptr) {
        LOG_ERROR << " event base obj null, return";
        return -1;
    }
//    LOG_INFO << "11111base=" << m_eventBase;
    int code = pthread_create(&m_tid, nullptr, &CEventLoopThread::thread_event_loop, this);
    LOG_INFO << "pthread_create m_tid=" << m_tid;
    return 0;
}
void* CEventLoopThread::thread_event_loop(void * arg) {
    CEventLoopThread* t = (CEventLoopThread*)(arg);

    struct event_base* base = t->get_eventbase();
    if (base == nullptr) {
        return nullptr;
    }
//    LOG_INFO << "2222base=" << base;

    pthread_mutex_lock(&t->m_mutex);
    t->m_isrun = true;
    pthread_mutex_unlock(&t->m_mutex);
    t->set_threadstatus(true);

    pthread_cond_signal(&t->m_cond);

    int code = event_base_loop(base, 0);
    if (code == 0) {
        LOG_INFO << "base_loop success";
    } else if (code == 1) {
        LOG_INFO << "no events were registered";
    } else {
        LOG_INFO << "something wrong";
    }

    LOG_INFO << "thread is finished";
    if (t == nullptr) {
        return nullptr;
    }
    t->set_threadstatus(false);
    return nullptr;
}

void CEventLoopThread::join() {
    LOG_INFO << "m_status=" << m_status << ", m_tid=" << m_tid;
    if (m_status) {
        pthread_join(m_tid, nullptr);
        m_status = false;
    }
}

short CEventLoopThread::set_eventcallback(event *e, int fd,
                                          short flags, event_callback_fn fn,
                                          void *arg,
                                          bool deleteoldflag, int second) {
    if (e == nullptr || m_eventBase == nullptr) {
        LOG_ERROR << ",tid:" << m_id << ",>>>>tmp," << flags << ",e NULL or m_eventBase NULL fd " << fd;
        return ERR_EVENT_FALSE;
    }

    if (deleteoldflag) {
        if (event_del(e) == -1) {
            LOG_ERROR << ",tid:" << m_id << ",>>>>tmp," << flags << ",event_del(e) -1";
            return -1;
        }
    }
    if (!flags) {
        LOG_ERROR << ",tid:" << m_id << ",>>>>tmp," << flags <<",flags 0 fd " << fd << " e " << e;
        return flags;
    }

    event_set(e, fd, flags, fn, arg);
    event_base_set(m_eventBase, e);
    if (second <= 0) {
        if (event_add(e, 0) == -1) {
            LOG_ERROR << ",tid:" << m_id << ",>>>>tmp," << flags << ",event_add -1 "  << fd << " e " << e;
            return -1;
        }
    } else {
        timeval t = {second, 0};
        if (event_add(e, &t) == -1) {
            LOG_ERROR << ",tid:" << m_id << ",>>>>tmp," << flags << ",event_add -1" << fd << " e " << e;
            return -1;
        }
    }
    LOG_INFO << ", tid:" << m_id << ", flags:" << flags << ", fd:" << fd
             << ", deleteoldflag:" << (int)deleteoldflag
             << ", second:" << second
             << ", e:" << e;
    return flags;
}
