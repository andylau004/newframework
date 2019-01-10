

#pragma once




#include <pthread.h>
#include <queue>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <mysql_error.h>
#include <cppconn/prepared_statement.h>



#include "common.h"




extern void tst_insert_badword();



typedef struct DBSourceInfo {
    std::string sqlAddr;    // "tcp://127.0.0.1:3306/IM_BUDDY"
    std::string user;
    std::string pwd;
    //Encodeing   enc;
    //std::string encoschemade;     // 编码格式 "utf8"
    //std::string ;     // 库
//    int8_t      readWrite;      // 1 write, 2 read

} DBSourceInfo;

class ConnctionPool {
public:
        ConnctionPool(int maxCount = 1);
        ~ConnctionPool();

        bool init(const DBSourceInfo& info);

        sql::Connection * getDBConnection();
        void retDBConnection(sql::Connection* conn);

        int GetCount();
//        bool insertFun();

        bool /*ConnctionPool::*/insertFun(const std::string& word, int oper);

private:
        pthread_mutex_t lock_;
        sql::Connection* createConnection();

        bool setDBSourceInfo(const DBSourceInfo& info);

private:
        static sql::Driver * driver_;
        DBSourceInfo *  info_;
        std::queue<sql::Connection*>  conns_;
        int maxCount_;

};






