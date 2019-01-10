#ifndef _THRIFT_CONNECTION_POOL_H_
#define _THRIFT_CONNECTION_POOL_H_

#include <boost/shared_ptr.hpp>

#include "exception.h"
#include <deque>


#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>


#include <thrift/transport/TSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/TApplicationException.h>

#include "utils.h"


template<typename TInterface>
class ThriftConnection;

template<typename TInterface>
class ThriftConnectionPoolBase : boost::noncopyable
{
public:
    virtual ~ThriftConnectionPoolBase(){
    }

    virtual boost::shared_ptr<ThriftConnection<TInterface> > get_connection(uint32_t ip, uint16_t port) throw (ErasureException) = 0;

    virtual void return_connection(TInterface*, uint32_t ip, uint16_t port) = 0;

    virtual void destroy_connection(TInterface*, uint32_t ip, uint16_t port) = 0;
};

template<typename TInterface>
class ThriftConnection : boost::noncopyable
{
public:
	typedef TInterface TClient;
    virtual ~ThriftConnection() {
        if (mPool != NULL && mClient != NULL) {
            mPool->return_connection(mClient, mIpAddr, mPort);
        }
    }

    ThriftConnection(ThriftConnectionPoolBase<TInterface> *pool, TInterface *client, uint32_t ip, uint16_t port) {
		mClient = client;
		mPool = pool;
		mIpAddr = ip;
		mPort = port;
    }

	TInterface* get_client() throw (ErasureException) {
        if (mClient != NULL)
        	return mClient;
        ErasureUtils::throw_exception(RESULT_ERR_GENERAL, "bug in thrift connection");
    }

    void destroy() {
    	if (mPool != NULL && mClient != NULL) {
    		mPool->destroy_connection(mClient, mIpAddr, mPort);
    		mClient = NULL;
    	}
    }

private:
    ThriftConnectionPoolBase<TInterface> *mPool;
    TInterface *mClient;
    uint32_t mIpAddr;
    uint16_t mPort;
};

template<typename TInterface, typename TClient>
class ThriftConnectionPool : public ThriftConnectionPoolBase<TInterface>
{
public:
	typedef std::pair<uint32_t, uint16_t> HOST;
	typedef std::deque<TInterface*> ConnIdle;
	typedef std::set<TInterface*> ConnActive;
	typedef ThriftConnection<TInterface> TThriftConnection;

	ThriftConnectionPool(uint32_t maxConn,
			uint32_t maxConnPerHost,
			uint32_t connTimeoutMs,
			uint32_t recvTimeoutMs,
			uint32_t sendTimeoutMs,
			uint32_t idleTimeoutSec,
            uint32_t blockWaitMs) :
        mTotalConn(0), mMaxConn(maxConn), mMaxConnPerHost(maxConnPerHost),
        mConnTimeoutMs(connTimeoutMs), mRecvTimeoutMs(recvTimeoutMs),
        mSendTimeoutMs(sendTimeoutMs), mIdleTimeoutMs(idleTimeoutSec), mBlockWaitMs(blockWaitMs) {
    }
	virtual ~ThriftConnectionPool() {
	}

public:
    virtual boost::shared_ptr<TThriftConnection> get_connection(uint32_t ip, uint16_t port) throw (ErasureException)
	{
		boost::mutex::scoped_lock lock(mMutex);
		TInterface *interf = NULL;

		HOST host(ip, port);
		init_connection_map(host);

		ConnIdle &connIdle = mIdleConns.find(host)->second;
		ConnActive &connActive = mActiveConns.find(host)->second;

		boost::system_time timeout;
		timeout = boost::get_system_time() + boost::posix_time::milliseconds(mBlockWaitMs);

		while (true)
		{
			if (!connIdle.empty()) {
				interf = connIdle.front();
				connIdle.pop_front();
				connActive.insert(interf);
				break;
			}

			uint32_t count = connIdle.size() + connActive.size();
			if (count < mMaxConnPerHost && mTotalConn < mMaxConn) {
				interf = new_client(ip, port);
				connActive.insert(interf);
				mTotalConn++;
				break;
			}

			if (!mCond.timed_wait(lock, timeout)) {
                std::cout/*LOG(ERROR)*/ << "timeout to get a connection to " << ip << " : " << port << std::endl;
				break;
			}
		}

		if (interf == NULL) {
			ErasureUtils::throw_exception(RESULT_ERR_RESOURCE_INSUFFICIENT, "conn unavailable");
		}

		return boost::shared_ptr<TThriftConnection>(new TThriftConnection(this, interf, ip, port));
	}

    virtual void return_connection(TInterface* interf, uint32_t ip, uint16_t port)
	{
		boost::mutex::scoped_lock lock(mMutex);

		HOST host(ip, port);
		typename std::map<HOST, ConnActive>::iterator itActive = mActiveConns.find(host);
		if (itActive == mActiveConns.end()) {
            std::cout/*LOG(ERROR)*/ << "invalid connection for " << ip << ":" << port << std::endl;
			return;
		}

		typename std::map<HOST, ConnIdle>::iterator itIdle = mIdleConns.find(host);
		if (itIdle == mIdleConns.end()) {
            std::cout/*LOG(ERROR)*/ << "invalid connection for " << ip << ":" << port << std::endl;
		}

		ConnActive &connActive = itActive->second;
		ConnIdle &connIdle = itIdle->second;

		connIdle.push_front(interf);
		connActive.erase(interf);

		mCond.notify_all();
	}

	virtual void destroy_connection(TInterface* interf, uint32_t ip, uint16_t port)
	{
		boost::mutex::scoped_lock lock(mMutex);

		HOST host(ip, port);
		typename std::map<HOST, ConnActive>::iterator it = mActiveConns.find(host);
		if (it == mActiveConns.end()) {
            std::cout/*LOG(ERROR)*/ << "invalid connection for " << ip << ":" << port << std::endl;
			return;
		}

		ConnActive &active = it->second;
		active.erase(interf);
		delete interf;
		mTotalConn--;
	}

private:
	void init_connection_map(const HOST &host)
	{
		if (mIdleConns.find(host) == mIdleConns.end())
			mIdleConns.insert(std::make_pair(host, ConnIdle()));

		if (mActiveConns.find(host) == mActiveConns.end())
			mActiveConns.insert(std::make_pair(host, ConnActive()));
	}

private:
	TInterface* new_client(uint32_t ip, uint16_t port) throw (ErasureException)
	{
		std::string host;
		ErasureUtils::ip2str(ip, /* out */host);
		if (host.empty()) {
			ErasureUtils::throw_exception(RESULT_ERR_INVALID_ARGUMENT, "invalid ip address");
		}
//        std::cout << "host=" << host << std::endl;

		boost::shared_ptr<apache::thrift::transport::TSocket> mSocket(new apache::thrift::transport::TSocket(host, port));
		mSocket->setConnTimeout(mConnTimeoutMs);
		mSocket->setRecvTimeout(mRecvTimeoutMs);
		mSocket->setSendTimeout(mSendTimeoutMs);
		boost::shared_ptr<apache::thrift::transport::TFramedTransport> mTransport(new apache::thrift::transport::TFramedTransport(mSocket));
		boost::shared_ptr<apache::thrift::protocol::TProtocol> mProtocol(new apache::thrift::protocol::TBinaryProtocol(mTransport));

		try
		{
			mTransport->open();
			return new TClient(mProtocol);
		}
		catch (const ::apache::thrift::transport::TTransportException& e)
		{
			throw ErasureException(RESULT_ERR_THRIFT_CONNECT_FAILED, e.what());
		}
		catch (const ::apache::thrift::TApplicationException& err)
		{
			throw ErasureException(RESULT_ERR_THRIFT_CONNECT_FAILED, err.what());
		}
	}

private:
	boost::mutex mMutex;
	boost::condition_variable mCond;
	std::map<HOST, ConnActive> mActiveConns;
	std::map<HOST, ConnIdle> mIdleConns;

	uint32_t mTotalConn;

	uint32_t mMaxConn;
	uint32_t mMaxConnPerHost;
	uint32_t mConnTimeoutMs;
	uint32_t mRecvTimeoutMs;
	uint32_t mSendTimeoutMs;
	uint32_t mIdleTimeoutMs;
	uint32_t mBlockWaitMs;
};

#endif /* _THRIFT_CONNECTION_POOL_H_ */
