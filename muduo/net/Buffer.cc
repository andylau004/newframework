// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Buffer.h>

#include <muduo/net/SocketsOps.h>

#include <errno.h>
#include <sys/uio.h>

using namespace muduo;
using namespace muduo::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;


/*
 * 从tcp缓冲区(sockfd)中读取数据，存放到应用层缓冲区中
 *   两种情况
 *      1.应用层缓冲区足以容纳所有数据
 *        直接读取到buffer_中
 *      2.应用层缓冲区不够
 *        开辟一段栈空间（128k）大小，使用分散读（readv）系统调用读取数据
 *        然后为buffer_开辟更大的空间，存放读到栈区的那部分数据
 *
 *   为什么不在Buffer构造时就开辟足够大的缓冲区
 *      1.每个tcp连接都有输入/输出缓冲区，如果连接过多则内存消耗会很大
 *      2.防止客户端与服务器端数据交互比较少，造成缓冲区的浪费
 *      3.当缓冲区大小不足时，利用vector内存增长的优势，扩充缓冲区
 *
 *   为什么不在读数据之前判断一下应用层缓冲区是否可以容纳内核缓冲区的全部数据
 *      1.采用这种方式就会调用一次recv，传入MSG_PEEK,即recv(sockfd,, extrabuf, sizeof(extrabuf), MSG_PEEK)
 *        可根据返回值判断缓冲区还有多少数据没有接收，然后再调用一次recv从内核冲读取数据
 *      2.但是这样会执行两次系统调用，得不偿失，尽量使用一次系统调用就将所有数据读出，这就需要一个很大的空间
 *
 *   struct iovec
 *      1.iov_base，存放数据的缓冲区起始位置，写时往这个位置写入iov_len个字节，读时从这个位置读出iov_len个字节
 *      2.iov_len，要读入多少数据从内核缓冲区/要写入多少数据到内核缓冲区
 *
 *   readv(int fd, const struct iovec *iov, int iovcnt);分散读
 *   writev(int fd, const struct iovec *iov, int iovcnt);集中写
 */
int/*ssize_t*/ Buffer::readFd(int fd, int* savedErrno)
{
    // saved an ioctl()/FIONREAD call to tell how much to read
    char         extrabuf[65536] = {0};
    struct iovec vec[2];

    /* 缓冲区接口，返回缓冲区还可以写入多少字节 */
    const size_t writable = writableBytes();

    /* 定义两块内存，一块是读缓冲区，一块是栈空间 */
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len  = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len  = sizeof extrabuf;

    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    /* 分散读，返回读取的字节数 */
    int n/*const ssize_t n*/ = (int)sockets::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;
    }
    /*
     * 读取的字节数比较少，读缓冲区足以容纳
     * 因为读缓冲区是readv的第一块内存，所以率先向这块内存写数据
     */
    else if (implicit_cast<size_t>(n) <= writable) {
        writerIndex_ += n;
    }
    else {
        /*
         * 将栈空间的数据追加到缓冲区末尾
         * 因为读缓冲区已经写满了，所以writerIndex指针就指向缓冲区的末尾
         */
        //使用了栈上空间，则将它append到buffer后面
        //前面handleRead里面的inputBuffer_.readFd函数也是经过精心设计的。
        //利用了栈上空间 + readv实现。这使得输入缓冲区足够大，一次readv就能取完全部数据。
        //这样节省了一个ioctl(sockfd, FIONREAD, &length)系统调用（该系统调用用来得知有多少数据可读，然后根据length在buffer中预留出足够的空间）
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    // if (n == writable + sizeof extrabuf)
    // {
    //   goto line_30;
    // }
    return n;
}

