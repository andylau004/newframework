/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _THRIFT_SERVER_TSERVER_H_
#define _THRIFT_SERVER_TSERVER_H_ 1

#include <thrift/TProcessor.h>
#include <thrift/transport/TServerTransport.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/concurrency/Thread.h>

#include <boost/shared_ptr.hpp>

namespace apache {
namespace thrift {
namespace server {

using apache::thrift::TProcessor;
using apache::thrift::protocol::TBinaryProtocolFactory;
using apache::thrift::protocol::TProtocol;
using apache::thrift::protocol::TProtocolFactory;
using apache::thrift::transport::TServerTransport;
using apache::thrift::transport::TTransport;
using apache::thrift::transport::TTransportFactory;

/**
 * Virtual interface class that can handle events from the server core. To
 * use this you should subclass it and implement the methods that you care
 * about. Your subclass can also store local data that you may care about,
 * such as additional "arguments" to these methods (stored in the object
 * instance's state).
 */
class TServerEventHandler {
public:
  virtual ~TServerEventHandler() {}

  /**
   * Called before the server begins.
   */
  virtual void preServe() {}

  /**
   * Called when a new client has connected and is about to being processing.
   */
  virtual void* createContext(boost::shared_ptr<TProtocol> input,
                              boost::shared_ptr<TProtocol> output) {
    (void)input;
    (void)output;
    return NULL;
  }

  /**
   * Called when a client has finished request-handling to delete server
   * context.
   */
  virtual void deleteContext(void* serverContext,
                             boost::shared_ptr<TProtocol> input,
                             boost::shared_ptr<TProtocol> output) {
    (void)serverContext;
    (void)input;
    (void)output;
  }

  /**
   * Called when a client is about to call the processor.
   */
  virtual void processContext(void* serverContext, boost::shared_ptr<TTransport> transport) {
    (void)serverContext;
    (void)transport;
  }

protected:
  /**
   * Prevent direct instantiation.
   */
  TServerEventHandler() {}
};

/**
 * Thrift server.
 *
 *  thrift核心库提供了一个TServer抽象类。

    TServer在thrift框架中的主要任务是接收client请求，并转发到某个processor上进行请求处理。
    针对不同的访问规模，thrift提供了不同TServer模型。
    
    thrift目前支持的server模型包括：
    TSimpleServer：使用阻塞io的单线程服务器，主要用于调试。
    TThreadedServer：使用阻塞io的多线程服务器，每一个请求都在一个线程中处理，并发访问情况下会有很多线程同时运行。
    TThreadPoolServer：使用阻塞io的多线程服务器，使用线程池管理处理线程。
    TNonBlockingServer：使用非阻塞io的多线程服务器，使用少量线程既可以完成大并发量的请求响应，
                        必须使用TFramedTransport。
    
    TServer对象通常如下工作：
    使用TServerTransport获得一个TTransport。
    使用TTransportFactory，可选地将原始传输转换为一个合适的应用传输。
    调用TProtocolFactory，为TTransport创建一个输入和输出。
    调用TProcessor对象的process方法。
 */
class TServer : public concurrency::Runnable {
public:
  virtual ~TServer() {}

  virtual void serve() = 0;

  virtual void stop() {}

  // Allows running the server as a Runnable thread
  virtual void run() { serve(); }

  boost::shared_ptr<TProcessorFactory> getProcessorFactory() { return processorFactory_; }

  boost::shared_ptr<TServerTransport> getServerTransport() { return serverTransport_; }

  boost::shared_ptr<TTransportFactory> getInputTransportFactory() { return inputTransportFactory_; }

  boost::shared_ptr<TTransportFactory> getOutputTransportFactory() {
    return outputTransportFactory_;
  }

  boost::shared_ptr<TProtocolFactory> getInputProtocolFactory() { return inputProtocolFactory_; }

  boost::shared_ptr<TProtocolFactory> getOutputProtocolFactory() { return outputProtocolFactory_; }

  boost::shared_ptr<TServerEventHandler> getEventHandler() { return eventHandler_; }

protected:
  TServer(const boost::shared_ptr<TProcessorFactory>& processorFactory)
    : processorFactory_(processorFactory) {
    setInputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setOutputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setInputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    setOutputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
  }

  TServer(const boost::shared_ptr<TProcessor>& processor)
    : processorFactory_(new TSingletonProcessorFactory(processor)) {
    setInputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setOutputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setInputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    setOutputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
  }

  TServer(const boost::shared_ptr<TProcessorFactory>& processorFactory,
          const boost::shared_ptr<TServerTransport>& serverTransport)
    : processorFactory_(processorFactory), serverTransport_(serverTransport) {
    setInputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setOutputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setInputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    setOutputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
  }

  TServer(const boost::shared_ptr<TProcessor>& processor,
          const boost::shared_ptr<TServerTransport>& serverTransport)
    : processorFactory_(new TSingletonProcessorFactory(processor)),
      serverTransport_(serverTransport) {
    setInputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setOutputTransportFactory(boost::shared_ptr<TTransportFactory>(new TTransportFactory()));
    setInputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    setOutputProtocolFactory(boost::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
  }

  TServer(const boost::shared_ptr<TProcessorFactory>& processorFactory,
          const boost::shared_ptr<TServerTransport>& serverTransport,
          const boost::shared_ptr<TTransportFactory>& transportFactory,
          const boost::shared_ptr<TProtocolFactory>& protocolFactory)
    : processorFactory_(processorFactory),
      serverTransport_(serverTransport),
      inputTransportFactory_(transportFactory),
      outputTransportFactory_(transportFactory),
      inputProtocolFactory_(protocolFactory),
      outputProtocolFactory_(protocolFactory) {}

  TServer(const boost::shared_ptr<TProcessor>& processor,
          const boost::shared_ptr<TServerTransport>& serverTransport,
          const boost::shared_ptr<TTransportFactory>& transportFactory,
          const boost::shared_ptr<TProtocolFactory>& protocolFactory)
    : processorFactory_(new TSingletonProcessorFactory(processor)),
      serverTransport_(serverTransport),
      inputTransportFactory_(transportFactory),
      outputTransportFactory_(transportFactory),
      inputProtocolFactory_(protocolFactory),
      outputProtocolFactory_(protocolFactory) {}

  TServer(const boost::shared_ptr<TProcessorFactory>& processorFactory,
          const boost::shared_ptr<TServerTransport>& serverTransport,
          const boost::shared_ptr<TTransportFactory>& inputTransportFactory,
          const boost::shared_ptr<TTransportFactory>& outputTransportFactory,
          const boost::shared_ptr<TProtocolFactory>& inputProtocolFactory,
          const boost::shared_ptr<TProtocolFactory>& outputProtocolFactory)
    : processorFactory_(processorFactory),
      serverTransport_(serverTransport),
      inputTransportFactory_(inputTransportFactory),
      outputTransportFactory_(outputTransportFactory),
      inputProtocolFactory_(inputProtocolFactory),
      outputProtocolFactory_(outputProtocolFactory) {}

  TServer(const boost::shared_ptr<TProcessor>& processor,
          const boost::shared_ptr<TServerTransport>& serverTransport,
          const boost::shared_ptr<TTransportFactory>& inputTransportFactory,
          const boost::shared_ptr<TTransportFactory>& outputTransportFactory,
          const boost::shared_ptr<TProtocolFactory>& inputProtocolFactory,
          const boost::shared_ptr<TProtocolFactory>& outputProtocolFactory)
    : processorFactory_(new TSingletonProcessorFactory(processor)),
      serverTransport_(serverTransport),
      inputTransportFactory_(inputTransportFactory),
      outputTransportFactory_(outputTransportFactory),
      inputProtocolFactory_(inputProtocolFactory),
      outputProtocolFactory_(outputProtocolFactory) {}

  /**
   * Get a TProcessor to handle calls on a particular connection.
   *
   * This method should only be called once per connection (never once per
   * call).  This allows the TProcessorFactory to return a different processor
   * for each connection if it desires.
   */
  boost::shared_ptr<TProcessor> getProcessor(boost::shared_ptr<TProtocol> inputProtocol,
                                             boost::shared_ptr<TProtocol> outputProtocol,
                                             boost::shared_ptr<TTransport> transport) {
    TConnectionInfo connInfo;
    connInfo.input = inputProtocol;
    connInfo.output = outputProtocol;
    connInfo.transport = transport;
    return processorFactory_->getProcessor(connInfo);
  }

  // Class variables
  boost::shared_ptr<TProcessorFactory> processorFactory_;//处理层的对象生成工厂对象
  boost::shared_ptr<TServerTransport> serverTransport_;//服务器传输对象

  boost::shared_ptr<TTransportFactory> inputTransportFactory_;//输入传输层工厂对象
  boost::shared_ptr<TTransportFactory> outputTransportFactory_;//输出传输层工厂对象

  boost::shared_ptr<TProtocolFactory> inputProtocolFactory_;//输入协议层工厂对象
  boost::shared_ptr<TProtocolFactory> outputProtocolFactory_;//输出协议层工厂对象

  boost::shared_ptr<TServerEventHandler> eventHandler_;//服务器事件处理对象类

public:
  void setInputTransportFactory(boost::shared_ptr<TTransportFactory> inputTransportFactory) {
    inputTransportFactory_ = inputTransportFactory;
  }

  void setOutputTransportFactory(boost::shared_ptr<TTransportFactory> outputTransportFactory) {
    outputTransportFactory_ = outputTransportFactory;
  }

  void setInputProtocolFactory(boost::shared_ptr<TProtocolFactory> inputProtocolFactory) {
    inputProtocolFactory_ = inputProtocolFactory;
  }

  void setOutputProtocolFactory(boost::shared_ptr<TProtocolFactory> outputProtocolFactory) {
    outputProtocolFactory_ = outputProtocolFactory;
  }

  void setServerEventHandler(boost::shared_ptr<TServerEventHandler> eventHandler) {
    eventHandler_ = eventHandler;
  }
};

/**
 * Helper function to increase the max file descriptors limit
 * for the current process and all of its children.
 * By default, tries to increase it to as much as 2^24.
 */
#ifdef HAVE_SYS_RESOURCE_H
int increase_max_fds(int max_fds = (1 << 24));
#endif
}
}
} // apache::thrift::server

#endif // #ifndef _THRIFT_SERVER_TSERVER_H_
