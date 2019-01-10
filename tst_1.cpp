

#include <sstream>

#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include <string.h>



using namespace std;

#include "tst_1.h"

#include "codeconvert.h"




#define MAXLINE 5
#define OPEN_MAX 100
#define LISTENQ 20
#define SERV_PORT 5000
#define INFTIM 1000


#define MAX_EVENT 20
#define READ_BUF_LEN 4096


/**
 * 设置 file describe 为非阻塞模式
 * @param fd 文件描述
 * @return 返回0成功，返回-1失败
 */
static int make_socket_non_blocking (int fd) {
    int flags, s;
    // 获取当前flag
    flags = fcntl(fd, F_GETFL, 0);
    if (-1 == flags) {
        perror("Get fd status");
        return -1;
    }

    flags |= O_NONBLOCK;

    // 设置flag
    s = fcntl(fd, F_SETFL, flags);
    if (-1 == s) {
        perror("Set fd status");
        return -1;
    }
    return 0;
}

int tst_epoll_1()
{
    // epoll 实例 file describe
    int epfd = 0;
    int listenfd = 0;
    int result = 0;
    struct epoll_event ev, event[MAX_EVENT];
    // 绑定的地址
    const char * const local_addr = "127.0.0.1";
    struct sockaddr_in server_addr = { 0 };

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listenfd) {
        perror("Open listen socket");
        return -1;
    }
    /* Enable address reuse */
    int on = 1;
    // 打开 socket 端口复用, 防止测试的时候出现 Address already in use
    result = setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    if (-1 == result) {
        perror ("Set socket");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    inet_aton (local_addr, &(server_addr.sin_addr));
    server_addr.sin_port = htons(9981);
    result = bind(listenfd, (const struct sockaddr *)&server_addr, sizeof (server_addr));
    if (-1 == result) {
        perror("Bind port");
        return 0;
    }
    result = make_socket_non_blocking(listenfd);
    if (-1 == result) {
        return 0;
    }

    result = listen(listenfd, 200);
    if (-1 == result) {
        perror("Start listen");
        return 0;
    }

    epfd = epoll_create1(0);
    if (1 == epfd) {
        perror("Create epoll instance");
        return 0;
    }

    ev.data.fd = listenfd;
    ev.events  = EPOLLIN | EPOLLET;//边缘触发选项
    // 设置epoll的事件
    result = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    if(-1 == result) {
        perror("Set epoll_ctl");
        return 0;
    }

    for ( ; ; ) {
        int wait_count;
        // 等待事件
        wait_count = epoll_wait(epfd, event, MAX_EVENT, -1);
        printf( "wait_count=%d\n", wait_count );

        for (int i = 0 ; i < wait_count; i++) {
            uint32_t events = event[i].events;
            // IP地址缓存
            char host_buf[NI_MAXHOST];
            // PORT缓存
            char port_buf[NI_MAXSERV];

            int __result;
            // 判断epoll是否发生错误
            if ( events & EPOLLERR || events & EPOLLHUP || (! events & EPOLLIN)) {
                printf("Epoll has error\n");
                close (event[i].data.fd);
                continue;
            } else if (listenfd == event[i].data.fd) {
                // listen的 file describe 事件触发， accpet事件
                for ( ; ; ) { // 由于采用了边缘触发模式，这里需要使用循环
                    struct sockaddr in_addr = { 0 };
                    socklen_t in_addr_len = sizeof (in_addr);
                    int accp_fd = accept(listenfd, &in_addr, &in_addr_len);
                    if (-1 == accp_fd) {
                        if (errno == EMFILE) {
                            printf( "accept failed, EMFILE sig happened\n" );
                            exit( -1 );
                        }
//                        perror("Accept");
                        printf("accept failed, accp_fd=%d errno=%d\n", accp_fd, errno);
                        break;
                    }
                    printf("accept successed, accp_fd=%d errno=%d\n", accp_fd, errno);
                    __result = getnameinfo(&in_addr, sizeof (in_addr),
                                           host_buf, sizeof (host_buf) / sizeof (host_buf[0]),
                                           port_buf, sizeof (port_buf) / sizeof (port_buf[0]),
                                           NI_NUMERICHOST | NI_NUMERICSERV);
                    if (! __result) {
                        printf("New connection: host = %s, port = %s\n", host_buf, port_buf);
                    }

                    __result = make_socket_non_blocking(accp_fd);
                    if (-1 == __result) {
                        printf( "set non blocking failed, fd=%d\n", accp_fd );
                        return 0;
                    }

                    ev.data.fd = accp_fd;
                    ev.events  = EPOLLIN | EPOLLET;

                    // 为新accept的 file describe 设置epoll事件
                    __result = epoll_ctl(epfd, EPOLL_CTL_ADD, accp_fd, &ev);
                    if (-1 == __result) {
                        perror("epoll_ctl");
                        return 0;
                    }

                }
                continue;
            } else {
                // 其余事件为 file describe 可以读取
                int done = 0;
                // 因为采用边缘触发，所以这里需要使用循环。如果不使用循环，程序并不能完全读取到缓存区里面的数据。
                for ( ; ; ) {
                    ssize_t result_len = 0;
                    char buf[READ_BUF_LEN] = { 0 };

                    result_len = read(event[i].data.fd, buf, sizeof (buf) / sizeof (buf[0]));
                    if (-1 == result_len) {
                        if (EAGAIN != errno) {
                            perror ("Read data");
                            done = 1;
                        }
                        break;
                    } else if (! result_len) {
                        done = 1;
                        break;
                    }

                    write(STDOUT_FILENO, buf, result_len);
                }
                if (done) {
                    printf("Closed connection\n");
                    close (event[i].data.fd);
                }
            }
        }

    }
    close (epfd);
    return 0;
}



int tst_epoll_2() {
    // epoll 实例 file describe
    int epfd = 0;
    int listenfd = 0;
    int result = 0;
    int wait_count = 0;

    struct epoll_event ev, array_events[MAX_EVENT];
    // 绑定的地址
    const char * const local_addr = "127.0.0.1";
    struct sockaddr_in server_addr = { 0 };

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listenfd) {
        perror("Open listen socket");
        return -1;
    }
    /* Enable address reuse */
    int on = 1;
    // 打开 socket 端口复用, 防止测试的时候出现 Address already in use
    result = setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    if (-1 == result) {
        perror ("Set socket");
        return 0;
    }

    server_addr.sin_family = AF_INET;
    inet_aton (local_addr, &(server_addr.sin_addr));
    server_addr.sin_port = htons(9981);
    result = bind(listenfd, (const struct sockaddr *)&server_addr, sizeof (server_addr));
    if (-1 == result) {
        perror("Bind port");
        return 0;
    }
    result = make_socket_non_blocking(listenfd);
    if (-1 == result) {
        return 0;
    }

    result = listen(listenfd, 200);
    if (-1 == result) {
        perror("Start listen");
        return 0;
    }

    epfd = epoll_create1(0);
    if (1 == epfd) {
        perror("Create epoll instance");
        return 0;
    }

    ev.data.fd = listenfd;
    ev.events  = EPOLLIN | EPOLLET;//边缘触发选项
    // 设置epoll的事件
    result = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    if (-1 == result) {
        perror("Set epoll_ctl");
        return 0;
    }

    for ( ; ; ) {
        // 等待事件
        wait_count = epoll_wait(epfd, array_events, MAX_EVENT, -1);
        printf( "wait_count=%d\n", wait_count );
        if( wait_count < 0 ) {
            printf( "epoll_wait failed! ret=%d", wait_count );
            exit(EXIT_FAILURE);
            return -1;
        }

        int activeFd = 0;
        int clientFd = 0;

        for (int i = 0 ; i < wait_count; i++) {
            uint32_t events = array_events[i].events;
            // IP地址缓存
            char host_buf[NI_MAXHOST];
            // PORT缓存
            char port_buf[NI_MAXSERV];

            int __result;

            activeFd = array_events[i].data.fd;

            if ( listenfd == activeFd ) {

                struct sockaddr in_addr = { 0 };
                socklen_t in_addr_len = sizeof (in_addr);

                while ( (clientFd = accept(listenfd, &in_addr, &in_addr_len) ) > 0 ) {
                    make_socket_non_blocking(clientFd);
                    ev.data.fd = clientFd;
                    ev.events  = EPOLLIN | EPOLLET;
                    __result = epoll_ctl( epfd, EPOLL_CTL_ADD, clientFd, &ev );
                    if (__result < 0)
                        exit(-1);
                }
//                if (clientFd < 0) {
//                    if (errno != EAGAIN || errno != )
//                }

                continue;//优先处理up
            }

            int nrecv = 0;
            if ( array_events[i].events & EPOLLIN ) {
                char recvBuff[READ_BUF_LEN] = { 0 };
                int  irecvsize = 0;

                while ( (nrecv = read(activeFd, recvBuff + irecvsize, (READ_BUF_LEN-1) - irecvsize ) ) > 0 ) {
                    irecvsize += nrecv;
                }
                if (nrecv == -1 && errno != EAGAIN) {
                    perror("read error");
                }

                ev.data.fd = activeFd;
                ev.events  = array_events[i].events | EPOLLOUT;
                __result = epoll_ctl(epfd, EPOLL_CTL_ADD, activeFd, &ev);
            }
            if ( array_events[i].events & EPOLLOUT ) {
                char respBuff[READ_BUF_LEN] = { 0 };
                sprintf(respBuff, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nHello World", 11);
                int nwrite, data_size = strlen(respBuff);

                int n = data_size;
                while ( n > 0 ) {
                    nwrite = write(activeFd, respBuff + data_size - n, n);
                    if (nwrite < n) {
                        if ( nwrite == -1 && errno != EAGAIN ) {
                            perror( "write data failed" );
                        }printf( "nwrite < n, nwrite=%d, n=%d\n", nwrite, n );
                        break;
                    }
                    n -= nwrite;
                }

            }


        }

    }
    close (epfd);
    return 0;
}

