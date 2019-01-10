

#pragma once



#include "muduo/base/Mutex.h"


#include <utility>

#include <stdio.h>
#include <unistd.h>

#include <vector>
#include <map>
#include <iostream>

#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>



using namespace muduo;
using namespace muduo::net;

using namespace std;



typedef struct _tag_MyStock{
    std::string    stockName;
    int            itemCnt;
}MyStock;

class CustomerData{
private:
    typedef vector<MyStock> StockList;
    typedef boost::shared_ptr<StockList> Stoptr;

    typedef map<std::string, StockList> Map;
    typedef boost::shared_ptr<Map> Maptr;

    Maptr      data_;
    MutexLock  mutex_;
    static int CopyTime;
    static int WriteTime;

public:
    CustomerData():data_(new Map){}
    void traverse(){
        Maptr tmp;
        {
            MutexLockGuard lock(mutex_);
            tmp=data_;
            std::cout<<"Copy Time="<<CopyTime<<endl;
            std::cout<<"Write Time="<<WriteTime<<endl;
            std::cout<<(double)CopyTime*100/WriteTime<<"%"<<endl;
        }
        for(Map::iterator p=data_->begin();p!=data_->end();++p){
            cout<<"-----------------------------"<<endl;
            cout<<"Customer:"<<p->first<<endl;
            for(StockList::iterator q=p->second.begin();q!=p->second.end();++q)
                cout<<"\t"<<q->stockName<<":"<<q->itemCnt<<endl;
            cout<<"-----------------------------"<<endl;
        }
    }

    int queryCnt(const std::string& customer,const std::string& stock){
        Maptr tmp;
        {
            MutexLockGuard lock(mutex_);
            tmp=data_;
        }
        Map::iterator p=tmp->find(customer);
        if(p==tmp->end()) {
            printf( "queryCnt not found customer=%s\n", customer.c_str() );
            return -1;
        }
        for(StockList::iterator q=p->second.begin();q!=p->second.end();++q) {
            if(q->stockName==stock) {
                return q->itemCnt;
            }
        }
        return -1;
    }

    void buyStock(const std::string& customer,const MyStock& stock){
        MutexLockGuard lock(mutex_);
        ++WriteTime;
        if(!data_.unique()){
            Maptr tmp(new Map(*data_));
            tmp.swap(data_);
            ++CopyTime;
        }
        assert(data_.unique());
//        Map::iterator p=data_->find(customer);
//        if(p==data_->end()){
//            (*data_)[customer].push_back(stock);
//            return;
//        }else{
//            for(StockList::iterator q=p->second.begin();q!=p->second.end();++q){
//                if(q->stockName==stock.stockName){
//                    q->itemCnt+=stock.itemCnt;
//                    return;
//                }
//            }
//            (*data_)[customer].push_back(stock);
//        }
    }

    void addCustomer(const std::string& customer){
        MutexLockGuard lock(mutex_);
        if(!data_.unique()){
            Maptr tmp(new Map(*data_));
            tmp.swap(data_);
        }
//        assert(data_.unique());
//        (*data_)[customer];
    }
};


int CustomerData::CopyTime=0;
int CustomerData::WriteTime=0;





