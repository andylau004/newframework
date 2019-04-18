


#include "tstsrv.h"


#include <stdio.h>
#include <unistd.h>
#include <sys/poll.h>


#include <list>


#include <fcntl.h>
#include <arpa/inet.h>
#include <assert.h>

#include <unordered_set>
#include <boost/circular_buffer.hpp>


#include "muduo/net/Callbacks.h"
#include "muduo/base/common.h"
#include "muduo/base/Logging.h"

#include "muduo/net/EventLoop.h"
#include "muduo/base/Timestamp.h"

#include "muduo/base/libevent.h"

#include "muduo/net/TcpServer.h"

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>


using namespace boost;

using namespace std;
using namespace muduo;
using namespace muduo::net;


#include <signal.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>

#include "tcpserver.h"



static const char MESSAGE[] = "Hello, World!\n";
static const int PORT = 9995;
static void listener_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *, int socklen, void *);
static void conn_writecb(struct bufferevent *, void *);
static void conn_eventcb(struct bufferevent *, short, void *);
static void signal_cb(evutil_socket_t, short, void *);



void tst_some_fun() {
    {
        int i = 0;
        const char** methods = event_get_supported_methods();

        printf("Starting Libevent %s.  Available methods are:\n",
               event_get_version());

        for (i=0; methods[i] != NULL; ++i) {
            printf("    %s\n", methods[i]);
        }
    }

    {
        struct event_base* evb;
        int/*enum event_method_feature*/ f;
        enum event_method_feature tmp_e;

        evb = event_base_new();
        if (!evb) {
            exit(2);
        } else {
            printf("Using Libevent with backend method %s.", event_base_get_method(evb));

            f = event_base_get_features(evb);
            if ((f & EV_FEATURE_ET))// 默认ET
                printf("  Edge-triggered events are supported.");
            if ((f & EV_FEATURE_O1))
                printf("  O(1) event notification is supported.");
            if ((f & EV_FEATURE_FDS))
                printf("  All FD types are supported.");
            puts("");
        }
    }

    {
        puts("init a event_base!");
        struct event_base *base; //定义一个event_base
        base = event_base_new(); //初始化一个event_base
        const char *x = event_base_get_method(base); //查看用了哪个IO多路复用模型，linux一下用epoll
        printf("METHOD:%s\n", x);
//        int y = event_base_dispatch(base); //事件循环。因为我们这边没有注册事件，所以会直接退出
        int y = event_base_loop(base, 0); //事件循环。因为我们这边没有注册事件，所以会直接退出
        event_base_free(base);  //销毁libevent
    }
}


void tst_null_event() {
    puts("init a event_base!");
    struct event_base *base; //定义一个event_base
    base = event_base_new(); //初始化一个event_base
    const char *x =  event_base_get_method(base); //查看用了哪个IO多路复用模型，linux一下用epoll
    printf("METHOD:%s\n", x);
//    int y = event_base_dispatch(base); //事件循环。因为我们这边没有注册事件，所以会直接退出
    int y = event_base_loop(base, 0); //事件循环。因为我们这边没有注册事件，所以会直接退出
    event_base_free(base);  //销毁libevent
}




int tst_libeventsrv_entry(int argc, char *argv[]) {
//    int tmp_array[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
//    LOG_INFO << "array_size(tmp_array)=" << array_size(tmp_array) * sizeof(int);

//    tst_null_event();
//    return 1;


    CTcpServer myserver;
    myserver.init(10091, 4);
    myserver.start();
    return 1;

    tst_some_fun();

    return 1;

    struct event_base *base;
    struct evconnlistener *listener;
    struct event *signal_event;
    struct sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    listener = evconnlistener_new_bind(base, listener_cb, (void*)base,
                                       LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
                                       (struct sockaddr *)&sin, sizeof(sin));
    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        return -1;
    }

    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *) base);
    if (!signal_event || event_add(signal_event, NULL) < 0) {
        fprintf(stderr, "Could not create/add a signal event!\n");
        return 1;
    }

    event_base_dispatch(base);
    evconnlistener_free(listener);
    event_free(signal_event);
    event_base_free(base);
    printf("done\n");
    return 0;
}

static void
listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *sa, int socklen, void *user_data) {
    struct event_base* base = (struct event_base*)user_data;
    struct bufferevent *bev;
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    LOG_INFO << "recv new clientfd=" << fd;

    bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_disable(bev, EV_READ);
    bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

static void
conn_writecb(struct bufferevent *bev, void *user_data) {
    struct evbuffer *output = bufferevent_get_output(bev);
    size_t len = evbuffer_get_length(output);
    if (len == 0) {
        LOG_INFO << ("flushed answer");
        bufferevent_free(bev);
    }
    LOG_INFO << "conn_writecb evbuffer_get_length len=" << len;
}

static void
conn_eventcb(struct bufferevent *bev, short events, void *user_data) {
    if (events & BEV_EVENT_EOF) {
        printf("Connection closed.\n");
    } else if (events & BEV_EVENT_ERROR) {
        printf("Got an error on the connection: %s\n", strerror(errno));/*XXX win32*/
    }
    LOG_INFO << "conn_eventcb recv events=" << events;
    // None of the other events can happen here, since we haven't enabled timeouts
    bufferevent_free(bev);
}

static void
signal_cb(evutil_socket_t sig, short events, void *user_data) {
    struct event_base *base = (struct event_base*)user_data;
    struct timeval delay = {2, 0};
    LOG_INFO << ("Caught an interrupt signal; exiting cleanly in two seconds.");

    event_base_loopexit(base, &delay);
}




