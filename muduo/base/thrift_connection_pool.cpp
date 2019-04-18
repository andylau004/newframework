
#include "muduo/base/common.h"

#include "thrift_connection_pool.h"




//template<typename TInterface, typename TClient>
//ThriftConnectionPool<TInterface, TClient>::ThriftConnectionPool(uint32_t maxConn,
//                                           uint32_t maxConnPerHost,
//                                           uint32_t connTimeoutMs,
//                                           uint32_t recvTimeoutMs,
//                                           uint32_t sendTimeoutMs,
//                                           uint32_t idleTimeoutSec,
//                                           uint32_t blockWaitMs) :
//    mTotalConn(0), mMaxConn(maxConn), mMaxConnPerHost(maxConnPerHost),
//    mConnTimeoutMs(connTimeoutMs), mRecvTimeoutMs(recvTimeoutMs),
//    mSendTimeoutMs(sendTimeoutMs), mIdleTimeoutMs(idleTimeoutSec), mBlockWaitMs(blockWaitMs)
//{
//    mTotalConn = 0;

//    mMaxConn = maxConn;
//    mMaxConnPerHost = maxConnPerHost;
//    mConnTimeoutMs = connTimeoutMs;
//    mRecvTimeoutMs = recvTimeoutMs;
//    mSendTimeoutMs = sendTimeoutMs;
//    mIdleTimeoutMs = idleTimeoutSec;
//    mBlockWaitMs = blockWaitMs;
//}
