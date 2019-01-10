

#include "mysqlinsert.h"



const static std::string cnst_BADWORD_TableName = "BADWORD";

sql::Driver * ConnctionPool::driver_ = NULL;




ConnctionPool::ConnctionPool(int maxCount):
    info_(NULL),
    maxCount_(maxCount) {
    pthread_mutex_init(&lock_, NULL);
}

ConnctionPool::~ConnctionPool() {
    pthread_mutex_destroy(&lock_);
    delete_object(info_);
}

bool ConnctionPool::init(const DBSourceInfo& info) {
    if (!setDBSourceInfo(info)) {
        printf( "set dbsource failed\n" );
        return false;
    }

    try {
        driver_ = get_driver_instance();
        if (NULL == driver_) {
//            LOG_ERROR("get_driver_instance faild.user:"<<info.user<<",sqlAddr"<<info.sqlAddr);
            printf( "get_driver_instance faild\n" );
            return false;
        }

//        LOG_INFO("conn maxCount_:"<<maxCount_);
        for (int i = 0; i < maxCount_; ++i) {
            sql::Connection* conn = createConnection();
            if (!conn) {
//                LOG_ERROR("createConnection faild.user:"<<info.user<<",sqlAddr:"<<info.sqlAddr);
                printf( "createConnection faild\n" );
                return false;
            }

            conns_.push(conn);
        }
    } catch (sql::SQLException &e) {
//        LOG_ERROR(" init ConnctionPool,"
//                    << e.getErrorCode()<<":"
//                    << e.what() << " "
//                    << info_->sqlAddr<< " "
//                    << info_->user);
        std::ostringstream oDbg;
        oDbg << "init ConnctionPool, err=" << e.getErrorCode() << "," << "&e.what=" << e.what()
             << "&source=" << info_->sqlAddr
             << "&user=" << info_->user;
        printf( "%s\n", oDbg.str().c_str() );
        oDbg.clear();

        return false;
    } catch(...) {
//        LOG_ERROR("init ConnctionPool throw.");
        printf("init ConnctionPool throw.\n");
        return false;
    }
    return true;
}

bool ConnctionPool::setDBSourceInfo(const DBSourceInfo& info) {
    delete_object(info_);

    info_ = new(std::nothrow) DBSourceInfo;
    if (NULL == info_) {
        return false;
    }

    info_->sqlAddr = info.sqlAddr;
    info_->user = info.user;
    info_->pwd = info.pwd;
//    info_->readWrite = info.readWrite;
    //maybe todo

    return true;
}

sql::Connection* ConnctionPool::createConnection() {
    if (NULL == info_) {
        return NULL;
    }

    try {
        sql::Connection* conn = NULL;
        sql::ConnectOptionsMap omap;

        omap["hostName"] = info_->sqlAddr;
        omap["OPT_CONNECT_TIMOUT"] = 3;
        omap["OPT_READ_TIMEOUT"] = 5;
        omap["OPT_WRITE_TIMEOUT"] = 5;
        omap["password"] = info_->pwd;
        omap["userName"] = info_->user;
        omap["OPT_RECONNECT"] = true;
//        con->setSchema("quickstartdb");
        omap["OPT_RECONNECT"] = true;

        conn = driver_->connect(omap);
        conn->setSchema("IM_BADWORD");
        return conn;
    } catch (sql::SQLException &e) {
        std::ostringstream oDbg;
        oDbg << "err=" << e.what() << ";" << e.getErrorCode()
             << "&source=" << info_->sqlAddr
             << "&user=" << info_->user;
        printf( "%s\n", oDbg.str().c_str() );
        oDbg.clear();

//        LOG_ERROR("CreateConnection,"
//                    << e.getErrorCode()<<":"
//                    << e.what() << " "
//                    << info_->sqlAddr<< " "
//                    << info_->user);
//        LOG_ERROR("err=" << e.what() << ";" << e.getErrorCode()
//                  << "&source=" << info_->sqlAddr
//                  << "&user=" << info_->user);
        return NULL;

    } catch(...) {
        return NULL;
    }
//    LOG_ERROR("err=unkown");
    return NULL;
}

sql::Connection * ConnctionPool::getDBConnection() {
    sql::Connection *conn = NULL;
    pthread_mutex_lock(&lock_);

    if (!conns_.empty()) {
        conn = conns_.front();
        conns_.pop();

        try {
            if (!conn->isValid()) {
                if (!conn->reconnect()) {
                    delete_object(conn);
                    conn = createConnection();
                }
            }
        } catch(...) {
            delete_object(conn);
            conn = createConnection(); //仍然试图在建立一个新连接
        }
    }
    pthread_mutex_unlock(&lock_);
    return conn;
}

void ConnctionPool::retDBConnection(sql::Connection* conn) {
    pthread_mutex_lock(&lock_);
    if (conn) {
        conns_.push(conn);
    }
    pthread_mutex_unlock(&lock_);
}

int ConnctionPool::GetCount() {
    int cout = 0;
    pthread_mutex_lock(&lock_);
    cout = conns_.size();
    pthread_mutex_unlock(&lock_);
    return cout;
}



bool ConnctionPool::insertFun(const std::string& word, int oper) {

    std::string addWord_ = "insert into @table (id, word, oper) values (?,?,?)";

    bool ret = false;
    sql::Connection* sqlConn = NULL;
    bool readWrite = false;

    try {
        std::string tn = cnst_BADWORD_TableName;
        sqlConn = getDBConnection();
        if (NULL == sqlConn) {
            printf("sqlConn is NULL\n");
            return false;
        }

        std::string tTmp = addWord_;

        boost::scoped_ptr<sql::PreparedStatement> pStmt(sqlConn->prepareStatement(tTmp.replace(tTmp.find("@table"), strlen("@table"), tn)));
        if (pStmt) {
            pStmt->setInt64(1, 1234);
            pStmt->setString(2, word);
            pStmt->setInt(3, oper);
            int sqlret = pStmt->executeUpdate();
//            LOG_DEBUG("sqlret=" << sqlret);
            printf("sqlret=%d\n", sqlret);
            ret = true;
        }
    } catch (sql::SQLException& e) {
//        LOG_ERROR(" err=" << e.getErrorCode() << "&what=" << e.what() << "&sqlerr=" << e.getSQLStateCStr());
        std::ostringstream oDbg;
        oDbg << "err=" << e.getErrorCode() << ";" << "&what=" << e.what()
             << "&source=" << info_->sqlAddr
             << "&user=" << info_->user;
        printf( "%s\n", oDbg.str().c_str() );
        oDbg.clear();
    } catch (...) {
//        LOG_ERROR("err=" << cnst_BADWORD_TableName <<"&id=" << id );
    }

//    retDBConnection(badWordShard_, cnst_BADWORD_TableName, readWrite, sqlConn);
    return true;
}


ConnctionPool g_cConnPool;



void tst_insert_badword() {

    DBSourceInfo tmpInfo;
    tmpInfo.sqlAddr = "127.0.0.1";
    tmpInfo.pwd  = "SKnFz.rkcF7IZjaS";
    tmpInfo.user = "qinghuitest";

    bool bret = g_cConnPool.init( tmpInfo );
    if (bret)
        printf( "connpoll init successed! cout=%d\n", g_cConnPool.GetCount() );

    g_cConnPool.insertFun( std::string("法轮功"), 1 );
//    g_cConnPool.setDBSourceInfo(tmpInfo);

}



