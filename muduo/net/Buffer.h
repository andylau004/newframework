// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_BUFFER_H
#define MUDUO_NET_BUFFER_H

#include <muduo/base/copyable.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>

#include <muduo/net/Endian.h>

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>
//#include <unistd.h>  // ssize_t

using namespace std;


namespace muduo
{
namespace net
{

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
/*
自己设计的可变缓冲区，成员变量vector<char>、readIndex、writeIndex，同时处理粘包问题。Buffer::readFd()中的extraBuffer通过堆上和栈上空间的结合，避免了内存资源的巨额开销。先加入栈空间再扩充和直接扩充的区别就是明确知道多少数据，避免巨大的buffer浪费并且减少read系统调用。

主要就是利用两个指针readerIndex，writerIndex分别记录着缓冲区中数据的起点和终点，写入数据的时候追加到writeIndex后面，读出数据时从readerIndex开始读。在readerIndex前面预留了几个字节大小的空间，方便日后为数据追加头部信息。缓冲区在使用的过程中会动态调整readerIndex和writerIndex的位置，初始缓冲区为空，readerIndex == writerIndex
 * */
/*
 *
 *   缓冲区的设计方法，muduo采用vector连续内存作为缓冲区，libevent则是分块内存
 *      1.相比之下，采用vector连续内存更容易管理，同时利用std::vector自带的内存
 *        增长方式，可以减少扩充的次数（capacity和size一般不同）
 *      2.记录缓冲区数据起始位置和结束位置，写入时写到已有数据的后面，读出时从
 *        数据起始位置读出
 *      3.起始/结束位置如上图的readerIndex/writeIndex，其中readerIndex为缓冲区
 *        数据的起始索引下标，writeIndex为结束位置下标。采用下标而不是迭代器的
 *        原因是删除(erase)数据时迭代器可能失效
 *      4.开头部分(readerIndex以前)是预留空间，通常只有几个字节的大小，可以用来
 *        写入数据的长度，解决粘包问题
 *      5.读出和写入数据时会动态调整readerIndex/writeIndex，如果没有数据，二者
 *        相等
 */
class Buffer : public muduo::copyable
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    }

    // implicit copy-ctor, move-ctor, dtor and assignment are fine
    // NOTE: implicit move-ctor is added in g++ 4.6

    void swap(Buffer& rhs)
    {
        buffer_.swap(rhs.buffer_);
        std::swap(readerIndex_, rhs.readerIndex_);
        std::swap(writerIndex_, rhs.writerIndex_);
    }
    /* 可读的数据就是起始位置和结束位置中间的部分 */
    size_t readableBytes() const
    { return writerIndex_ - readerIndex_; }

    size_t writableBytes() const
    { return buffer_.size() - writerIndex_; }

    size_t prependableBytes() const
    { return readerIndex_; }

    /* 返回数据起始位置 */
    const char* peek() const
    { return begin() + readerIndex_; }

    const char* findCRLF() const
    {
        // FIXME: replace with memmem()?
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findCRLF(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= beginWrite());
        // FIXME: replace with memmem()?
        const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findEOL() const
    {
        const void* eol = memchr(peek(), '\n', readableBytes());
        return static_cast<const char*>(eol);
    }

    const char* findEOL(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const void* eol = memchr(start, '\n', beginWrite() - start);
        return static_cast<const char*>(eol);
    }

    // retrieve returns void, to prevent
    // string str(retrieve(readableBytes()), readableBytes());
    // the evaluation of two functions are unspecified
    /* 调整readerIndex，后移len */
    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        /*
         * 如果调整后仍然有数据，就将readerIndex增加len
         * 如果已经将数据全部读完(len >= readableBytes)，那么就初始化readerIndex/writerIndex位置
         */
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }

    void retrieveUntil(const char* end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveInt64()
    {
        retrieve(sizeof(int64_t));
    }

    void retrieveInt32()
    {
        retrieve(sizeof(int32_t));
    }

    void retrieveInt16()
    {
        retrieve(sizeof(int16_t));
    }

    void retrieveInt8()
    {
        retrieve(sizeof(int8_t));
    }

    //如果数据全部被用户读出，就重新调整readerIndex/writerIndex位置
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    /* 从缓冲区中读取len个字节的数据 */
    string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    string retrieveAsString(size_t len)
    {
        assert(len <= readableBytes());
        /* peek返回数据的起点 */
        /* 调用string(const char* s, size_type n);构造函数，初始化为从地址s开始的n个字节 */
        string result(peek(), len);
        /* 调整缓冲区，即改变readerIndex的位置，后移len */
        retrieve(len);
        return result;
    }

    StringPiece toStringPiece() const
    {
        return StringPiece(peek(), static_cast<int>(readableBytes()));
    }

    void append(const StringPiece& str)
    {
        append(str.data(), str.size());
    }

    //函数首先调用ensureWritableBytes函数确保读缓冲区有足够的空间，如果没有，就需要调用resize函数重新设置空间大小（std::vector的内存增长就体现在这里，因为capacity和size通常不同，所以如果resize设置的大小没有超过capacity，那么空间仍然足够，不会重新开辟内存，将数据拷贝到新内存上）
    void append(const char* /*restrict*/ data, size_t len)
    {
        /* 确保有足够的空间容纳len大小的数据 */
        ensureWritableBytes(len);
        /* 将数据copy到writerIndex后面，beginWrite返回的就是writerIndex位置的地址（writerIndex是下标） */
        std::copy(data, data+len, beginWrite());
        /* 写完数据，更新writerIndex */
        hasWritten(len);
    }

    void append(const void* /*restrict*/ data, size_t len)
    {
        append(static_cast<const char*>(data), len);
    }

    void ensureWritableBytes(size_t len)
    {
        /* 返回剩余可用空间大小，如果不足len，开辟新空间（调用resize） */
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    char* beginWrite()
    { return begin() + writerIndex_; }

    const char* beginWrite() const
    { return begin() + writerIndex_; }

    void hasWritten(size_t len)
    {
        assert(len <= writableBytes());
        writerIndex_ += len;
    }

    void unwrite(size_t len)
    {
        assert(len <= readableBytes());
        writerIndex_ -= len;
    }

    ///
    /// Append int64_t using network endian
    ///
    void appendInt64(int64_t x)
    {
        int64_t be64 = sockets::hostToNetwork64(x);
        append(&be64, sizeof be64);
    }

    ///
    /// Append int32_t using network endian
    ///
    void appendInt32(int32_t x)
    {
        int32_t be32 = sockets::hostToNetwork32(x);
        append(&be32, sizeof be32);
    }

    void appendInt16(int16_t x)
    {
        int16_t be16 = sockets::hostToNetwork16(x);
        append(&be16, sizeof be16);
    }

    void appendInt8(int8_t x)
    {
        append(&x, sizeof x);
    }

    ///
    /// Read int64_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    int64_t readInt64()
    {
        int64_t result = peekInt64();
        retrieveInt64();
        return result;
    }

    ///
    /// Read int32_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    int32_t readInt32()
    {
        int32_t result = peekInt32();
        retrieveInt32();
        return result;
    }

    int16_t readInt16()
    {
        int16_t result = peekInt16();
        retrieveInt16();
        return result;
    }

    int8_t readInt8()
    {
        int8_t result = peekInt8();
        retrieveInt8();
        return result;
    }

    ///
    /// Peek int64_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int64_t)
    int64_t peekInt64() const
    {
        assert(readableBytes() >= sizeof(int64_t));
        int64_t be64 = 0;
        ::memcpy(&be64, peek(), sizeof be64);
        return sockets::networkToHost64(be64);
    }

    ///
    /// Peek int32_t from network endian
    ///
    /// Require: buf->readableBytes() >= sizeof(int32_t)
    int32_t peekInt32() const
    {
        assert(readableBytes() >= sizeof(int32_t));
        int32_t be32 = 0;
        ::memcpy(&be32, peek(), sizeof be32);
        return sockets::networkToHost32(be32);
    }

    int16_t peekInt16() const
    {
        assert(readableBytes() >= sizeof(int16_t));
        int16_t be16 = 0;
        ::memcpy(&be16, peek(), sizeof be16);
        return sockets::networkToHost16(be16);
    }

    int8_t peekInt8() const
    {
        assert(readableBytes() >= sizeof(int8_t));
        int8_t x = *peek();
        return x;
    }

    ///
    /// Prepend int64_t using network endian
    ///
    void prependInt64(int64_t x)
    {
        int64_t be64 = sockets::hostToNetwork64(x);
        prepend(&be64, sizeof be64);
    }

    ///
    /// Prepend int32_t using network endian
    ///
    void prependInt32(int32_t x)
    {
        int32_t be32 = sockets::hostToNetwork32(x);
        prepend(&be32, sizeof be32);
    }

    void prependInt16(int16_t x)
    {
        int16_t be16 = sockets::hostToNetwork16(x);
        prepend(&be16, sizeof be16);
    }

    void prependInt8(int8_t x)
    {
        prepend(&x, sizeof x);
    }

    void prepend(const void* /*restrict*/ data, size_t len)
    {
        assert(len <= prependableBytes());
        readerIndex_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d+len, begin()+readerIndex_);
    }

    //收缩空间，保留reserver个字节，可能多次读写后buffer太大了，可以收缩
    void shrink(size_t reserve)
    {
        // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
        //为什么要使用Buffer类型的other来收缩空间呢?如果不用这种方式，我们可选的有使用resize()，把我们在resize()z和
        Buffer other;//生成临时对像，保存readable内容，然后和自身交换，该临时对象再析构掉
        //ensureWritableBytes()函数有两个功能，一个是空间不够resize空间，一个是空间足够内部腾挪，这里明显用的是后者。
        other.ensureWritableBytes(readableBytes()+reserve);//确保有足够的空间，内部此时已经腾挪
        other.append(toStringPiece());//把当前数据先追加到other里面，然后再交换
        swap(other);//然后再交换
    }

    size_t internalCapacity() const
    {
        return buffer_.capacity();
    }

    /// Read data directly into buffer.
    ///
    /// It may implement with readv(2)
    /// @return result of read(2), @c errno is saved
    ///
    /* 从套接字（内核tcp缓冲区）中读取数据放到读缓冲区中 */
    int/*ssize_t*/ readFd(int fd, int* savedErrno);

private:

    char* begin()
    { return &*buffer_.begin(); }

    const char* begin() const
    { return &*buffer_.begin(); }

    void makeSpace(size_t len)
    {
        /*
         * 在多次从缓冲区读数据后，readerIndex会后移很多，导致预留空间变大
         * 在增大空间之前，先判断调整预留空间的大小后能否容纳要求的数据
         * 如果可以，则将预留空间缩小为8字节（默认的预留空间大小）
         * 如果不可以，那么就只能增加空间
         */
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            // FIXME: move readable data
            /* writerIndex代表当前缓冲区已使用的大小，调整只需调整到恰好满足len大小即可 */
            buffer_.resize(writerIndex_+len);
        }
        else
        {
            /* 通过缩小预留空间大小可以容纳len个数据，就缩小预留空间 */
            // move readable data to the front, make space inside buffer
            assert(kCheapPrepend < readerIndex_);// 如果可以压缩的话那么就压缩
            /* 返回缓冲区数据个数，writerIndex - readerIndex */
            size_t readable = readableBytes();
            /* 将所有数据前移 */
            std::copy(begin()+readerIndex_,
                      begin()+writerIndex_,
                      begin()+kCheapPrepend);
            /* 更新两个指针（下标） */
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());
        }
    }

private:
    /* 缓冲区 */
    std::vector<char> buffer_;
    /* 数据起始点 */
    size_t readerIndex_;
    /* 数据结束点 */
    size_t writerIndex_;

    static const char kCRLF[];
};

}
}

#endif  // MUDUO_NET_BUFFER_H
