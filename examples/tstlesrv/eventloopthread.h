

#pragma once


#include <stdio.h>
#include <event.h>

#include <pthread.h>



class CEventLoopThread {

public:
    CEventLoopThread( int tId );
    ~CEventLoopThread();

public:
    struct event_base* get_eventbase() { return m_eventBase; }
    pthread_t get_ptid() const { return m_tid; }
    void set_threadstatus(bool isrun) { m_status = isrun; }

    int  start(bool isasync = true);
    void join();

    /**
     *  注册事件
     *  @param e, event
     *  @param fd, file descriptor
     *  @param flags, event flags
     *  @param fn, event call back
     *  @param arg, call back param
     *  @param deleteoldflag, is delete event old flags
     *  @param return new flags or error
     */
    short set_eventcallback(struct event * e,
                            evutil_socket_t fd,
                            short flags,
                            event_callback_fn fn,
                            void *arg,
                            bool deleteoldflag = false,
                            int second = 0);

private:

private:
    void init();
    void init_eventbase();
    void destroy();

    int create_thread();
    static void * thread_event_loop(void * arg);

private:
    bool m_status;    /**< thread status */
    int  m_id;        /**< identifer */
    pthread_t m_tid;  /**< pthread id */

    pthread_mutex_t m_mutex;
    pthread_cond_t  m_cond;
    bool            m_isrun;

    struct event_base * m_eventBase;

};


