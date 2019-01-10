#ifndef TST_PRJ_BASE_COMMON_H_
#define TST_PRJ_BASE_COMMON_H_











#include <stdio.h>
#include <string>
#include <stdint.h>


#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include <errno.h>

#include <iostream>
#include <sstream>
#include <fstream>

#include <sys/stat.h>

#include <limits.h>
#include <time.h>

#include <algorithm>
#include <sys/time.h>
#include <ctype.h>

#include <set>
#include <map>
#include <list>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <functional>

#include <unordered_set>
#include <boost/circular_buffer.hpp>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/any.hpp>
#include <boost/unordered_map.hpp>

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

//#include "thirdparty/glog/logging.h"

//#include "codec.h"
#include "utils.h"
//#include "md5lib.h"
#include "errcode.h"
//#include "configs.h"
//#include "rw_lock.h"
#include "exception.h"
//#include "thread_pool.h"
#include "thrift_connection_pool.h"



using namespace std;



#define MAX_FILE_NAME_LEN 31

#define LOG_INFO_SMART LOG_IF(INFO, (FLAGS_minloglevel <= google::INFO))
#define LOG_WARNING_SMART LOG_IF(WARNING, (FLAGS_minloglevel <= google::WARNING))

#define CHECK_RET(ret, msg) do { if (!RESULT_IS_SUCCESS(ret)) \
        ErasureUtils::throw_exception(ret, msg); \
        } while(0)

#define CHECK_TRUE(exp, msg) do { if (!(exp)) \
        ErasureUtils::throw_exception(RESULT_ERR_GENERAL, msg); \
        } while(0)


enum ServerStatus {
    Server_Starting = 0,
    Server_Running = 1,
    Server_Suspended = 2,
    Server_Dead = 3,
};

/*
 * 0       7|8     11|12      31
 * +++++++++++++++++++++++++++++
 * | nodeid | diskid |  seqid  |
 * +++++++++++++++++++++++++++++
 */
#define NODE_ID_BITS 8
#define DISK_ID_BITS 4
#define SEQ_ID_BITS 20
#define CHUNK_ID_BITS 32

#define NODE_ID_OFFSET (CHUNK_ID_BITS - NODE_ID_BITS)
#define DISK_ID_OFFSET (NODE_ID_OFFSET - DISK_ID_BITS)

#define NODE_ID(chunkid) (int16_t)(((uint32_t)chunkid >> NODE_ID_OFFSET) & ((1 << NODE_ID_BITS) - 1))
#define DISK_ID(chunkid) (int8_t)(((uint32_t)chunkid >> DISK_ID_OFFSET) & ((1 << DISK_ID_BITS) - 1))
#define SEQ_ID(chunkid) (int32_t)(chunkid & ((1 << SEQ_ID_BITS) - 1))

#define MAKE_CHUNK_ID(nodeid, diskid, seqid) (nodeid << NODE_ID_OFFSET | diskid << DISK_ID_OFFSET | seqid)

// -------------------------------------------------------------------------------------------------------





#ifdef H_OS_WINDOWS
#define evpp_socket_t intptr_t
#else
#define evpp_socket_t int
#endif



#define UNUSED(x) ((void)x)

#define array_size(arr)    (sizeof(arr)/sizeof(arr[0]))


#define VRV_ERR -1
#define VRV_OK  0

#define HYPHEN "-"
#define SEP    "_"


typedef int SOCKET;
#define INVALID_SOCKET -1


#define UNUSED(x) ((void)x)

template <class T>
void delete_object(T *& t) {
    if (t) {
        delete t;
        t = NULL;
    }
}

template <class T>
void delete_object_array(T ** t) {
    if (t && *t) {
        delete [] (*t);
        (*t) = NULL;
    }
}

typedef struct _tag_OutputDbgInfo {
    _tag_OutputDbgInfo( const std::string& str_in, const std::string& str_out );
    _tag_OutputDbgInfo( const char* str_in, const char* str_out );

    ~_tag_OutputDbgInfo( );

    std::string  m_str_out;
}OutputDbgInfo;



template<class CONTAINER>
void DeleteSecondPtrInContainer( CONTAINER& container ) {
    if ( container.empty() ) {
        return;
    }
    for( /*CONTAINER::iterator*/auto pIt = container.begin(); pIt != container.end(); ++ pIt ) {
        SafeDelete( pIt->second );
    }
    container.clear();
}

template<class CONTAINER>
void PrintInContainer( CONTAINER& container ) {
    if ( container.empty() ) {
        return;
    }
    for( /*CONTAINER::iterator*/ auto itElem = container.begin(); itElem != container.end(); ++ itElem ) {
        std::cout << " " << *itElem ;
    }
    printf( "\n" );
}

/*!
 * \brief The Align enum
 */
enum ALIGN_MEMORY {
    __ALIGN = 8 /*!< default align */
};

/*!
 * \brief holly_align
 * \param bytes
 * \param align
 * \return aligned bytes
 */
inline size_t holly_align(size_t bytes, size_t align) {
    return  (((bytes) + (align - 1)) & ~(align - 1));
}



#define IDLE_CTX_TIMEOUT 60   // 1 min
#define IDLE_CTX_REMOVE  5    //remove idle_ctx numbers once

#define ERR_EVENT_BASE -1
#define ERR_EVENT_FALSE -2
#define SUCCESS 0

#define ERR_EAGAIN EAGAIN
#define ERR_EWOULDBLOCK EWOULDBLOCK
#define ERR_EINVAL EINVAL
#define ERR_EINTR EINTR

#define ERR_ECONNRESET ECONNRESET
#define ERR_ENOTCONN ENOTCONN
#define ERR_ETIMEDOUT ETIMEDOUT
#define ERR_EPIPE EPIPE


#define MAX_THREAD_COUNT 8

#define LISTEN_BACKLOG_AP 512


enum TypeProcess {
    P_UNKNOWN = 0,
    P_TEST_PROCESS

};


enum TypeProtocol {
    PRO_UNKNOWN = 0,
    PRO_BINARY,
    PRO_JSON
};

const int32_t g_basic_timestamp_type[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 21 };
const int     g_basic_timestamp_size   = sizeof(g_basic_timestamp_type) / sizeof(int32_t);

const int32_t g_basic_ts_type[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 21, 23 };
const int     g_basic_ts_size   = sizeof(g_basic_ts_type) / sizeof(int32_t);


#define SETVALUE(b, t, i, v) if (b) {stmt->set##t((i),(v));}
#define SETVALUE_EX(b, t, i, v1, v2) if (b) {stmt->set##t((i),(v1));} else {stmt->set##t((i),(v2));}
#define SPLICESQL(b, x, y) if (b) { (x) += (y);}
#define SPLICESQL_EX(b, x, y, z) if (b) { (x) += (y); ++(z);}

#define DMYSQLPJ(b,c,x,y) if(b) { (x) += (y); c = true;}

#define REMOVE_LAST_COMMA(x) { size_t pos = x.find_last_of(','); \
                                if (pos == std::string::npos) return false; (x) = (x).substr(0, pos); }

inline void remove_last_comma(std::string& str) {
    size_t pos = str.find_last_of(',');
    if (pos != std::string::npos) {
        str = str.substr(0, pos);
    }
}

//#define UNSUPPORTSERVICECODE 11102


#define URGENT_SERVICE_ID   10001L
#define system_sendID       10000L



#include "thirdpart/rapidjson/document.h"
#include "thirdpart/rapidjson/stringbuffer.h"
#include "thirdpart/rapidjson/writer.h"
#include "thirdpart/rapidjson/document.h"
#include "thirdpart/rapidjson/filereadstream.h"


//#include <log4cplus/loggingmacros.h>
//#include <log4cplus/configurator.h>

//#include <log4cplus/logger.h>
//#include <log4cplus/helpers/loglog.h>
//#include <log4cplus/helpers/stringhelper.h>


#include "timerutil.h"




// convert<int8_t> 获取到ASCII码
template <typename out, class in>
out convert(const in & value) {
    std::stringstream stream;
    stream << value;
    out r;
    stream >> r;
    return r;
}

// convert 获取到整数值
template <class in>
int8_t convert(const in & value) {
    std::stringstream stream;
    stream << value;
    int16_t r;
    stream >> r;
    return (int8_t)r;
}

// 针对int8, 单独写的模板
template <class out>
out int8_to_type(const int8_t& value) {
    int v = (int)value;
    std::stringstream stream;
    stream << v;
    out r;
    stream >> r;
    return r;
}

template <class in>
int8_t type_to_int8(const in& value) {
    std::stringstream stream;
    stream << value;
    int r;
    stream >> r;
    return (int8_t)r;
}

template <class T>
T get_value(const std::string & str) {
    std::istringstream iss(str);

    T value;
    iss >> value;
    return value;
}


bool        stringTrimEx( std::string& sInput, char c = ' ' );
//	转换为大写
void        MakeUpper( std::string& sInput ) ;

//	转换为小写
void        MakeLower( std::string& sInput ) ;



bool CreateFullDir(const char* lpszdir);
bool CreateFullDir(const std::string& indir);


bool isFileExist(const std::string& infile);
bool isFileExist(const char* lpszfile);


bool isPathExist(const char* lpszdir);
bool isPathExist(const std::string& indir);



std::string GetCurrentDir();




bool CopyFileEx( const char* lpszSrc, const char* lpszDst );
bool CopyFileEx( const std::string& srcfile, const std::string& dstfile );


bool save_file( const char* lpfilename, const std::string& sinput );
bool save_file( const char* lpfilename, char* inbuffer, size_t insize );


std::string GetFileContent_string( const std::string& inFileName );
std::string GetFileContent_string( const char* lpszFileName );
char* GetSmallFileContent( const char* lpszFileName, unsigned int* pFileSize );
unsigned int GetFileSize( const char* lpszFileName );



bool get_string_value(const rapidjson::Document& d, const char* name, std::string& value);

std::string SplitString( const std::string& s, std::set<std::string>& v, const std::string& c );
std::string SplitString( const std::string& s, std::set<std::string>& v, const char* lpszsep );




void GetStringFromJson( const rapidjson::Document& jsonReader, const char* szName, std::string& outString, const char* lpszDefault );
void GetInt64FromJson( const rapidjson::Document& jsonReader, const char* szName, int64_t& outVal, int64_t inDefault );
void GetIntFromJson( const rapidjson::Document& jsonReader, const char* szName, int32_t& outVal, int32_t inDefault );




uint64_t stringToUint64_t(const char* lpszVal);



//ConfigurationUtil& G_GetConfigObj();


uint64_t GetCurMilliSecond();
std::string GetCurMilliSecond_string();

int64_t  GetRandom64();

int CurTid();


///check time function
class checktime{
public:
    checktime(const std::string &name, const int64_t boundary):
        _boundary(boundary), _name(name),_info("") {
#if 0
        gettimeofday(&_starttime, 0);
#endif
        start_ = TimerUtils::get_current_nanotime();
    }
    checktime(const std::string &name, const std::string &info,const int64_t boundary):
        _boundary(boundary), _name(name),_info(info) {
#if 0
        gettimeofday(&_starttime, 0);
#endif
        start_ = TimerUtils::get_current_nanotime();
    }
    ~checktime() {
        check_dump("");
    }

    void check(const std::string &info){
        check_dump(info);
    }

private:
    void check_dump(const std::string &info){
#if 0
        struct timeval end;
        gettimeofday(&end, 0);
        //ms
        int32_t diff = ((end.tv_sec * 1000000 + end.tv_usec) - (_starttime.tv_sec*1000000 + _starttime.tv_usec))/1000 ;
#endif
        int32_t diff = (int32_t)((TimerUtils::get_current_nanotime() - start_) / 1e6);
        dump(_name, _info, info, _boundary, diff);
    }
    void dump(const std::string& name, const std::string& _info, const std::string& info, const int32_t _boundary, const int32_t diff);

    struct timeval _starttime;
    int64_t start_;
    int32_t _boundary; //ms
    std::string _name;
    std::string _info;
};




#define CHECK_TIME() \
    ::checktime _check_time(__FUNCTION__, 200);//default 200ms
#define CHECK_TIME1(self_def) \
    ::checktime self_def(__FUNCTION__, #self_def, 200);//default 200ms


#define __SCOPEGUARD_CONCATENATE_IMPL(s1, s2) s1##s2
#define __SCOPEGUARD_CONCATENATE(s1, s2) __SCOPEGUARD_CONCATENATE_IMPL(s1, s2)

#if defined(__cplusplus)

#include <type_traits>

// ScopeGuard for C++11
namespace clover {
    template <typename Fun>
    class ScopeGuard {
    public:
        ScopeGuard(Fun &&f) : _fun(std::forward<Fun>(f)), _active(true) {
        }

        ~ScopeGuard() {
            if (_active) {
                _fun();
            }
        }

        void dismiss() {
            _active = false;
        }

        ScopeGuard() = delete;
        ScopeGuard(const ScopeGuard &) = delete;
        ScopeGuard &operator=(const ScopeGuard &) = delete;

        ScopeGuard(ScopeGuard &&rhs) : _fun(std::move(rhs._fun)), _active(rhs._active) {
            rhs.dismiss();
        }

    private:
        Fun _fun;
        bool _active;
    };

    namespace detail {
        enum class ScopeGuardOnExit {};

        template <typename Fun>
        inline ScopeGuard<Fun> operator+(ScopeGuardOnExit, Fun &&fn) {
            return ScopeGuard<Fun>(std::forward<Fun>(fn));
        }
    } // namespace detail
} // namespace clover

// Helper macro
#define ON_SCOPE_EXIT \
    auto __SCOPEGUARD_CONCATENATE(ext_exitBlock_, __LINE__) = clover::detail::ScopeGuardOnExit() + [&]()

#else

// ScopeGuard for Objective-C
typedef void (^ext_cleanupBlock_t)(void);
static inline void ext_executeCleanupBlock(__strong ext_cleanupBlock_t *block) {
    (*block)();
}

#define ON_SCOPE_EXIT                                                              \
    __strong ext_cleanupBlock_t __SCOPEGUARD_CONCATENATE(ext_exitBlock_, __LINE__) \
        __attribute__((cleanup(ext_executeCleanupBlock), unused)) = ^

#endif




#define UNIQUE_LOCK_EX(oneMtx)  boost::unique_lock<boost::mutex> tmpMtx(oneMtx);



#include "muduo/base/Logging.h"

template <class T>
void printXto_console(const char* lpszPrefix, T& x) {
    std::ostringstream stream;
    x.printTo(stream);
//    printf( "tostrings=%s\n", stream.str().c_str() );
    LOG_INFO << lpszPrefix << " tostrings=" << stream.str().c_str();
}







#endif  // TST_PRJ_BASE_COMMON_H_

