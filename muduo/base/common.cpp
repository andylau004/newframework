
#include "common.h"



#include <stdio.h>
#include <string>
#include <stdint.h>

#include <errno.h>

#include <iostream>
#include <sstream>
#include <fstream>


//#include "vrv_common_log.h"
//#include "logs/log.h"
#include "Logging.h"

#include <random>

#include "muduo/base/CurrentThread.h"

using namespace std;
using namespace muduo;
//using namespace muduo::net;


_tag_OutputDbgInfo::_tag_OutputDbgInfo( const std::string& str_in, const std::string& str_out ) {
    m_str_out = str_out;
    printf( "\n" );
//    printf( "--------------------%s\n", str_in.c_str() );
    LOG_INFO << "--------------------" << str_in;
}
_tag_OutputDbgInfo::_tag_OutputDbgInfo( const char* str_in, const char* str_out ) {
    m_str_out = str_out ;
    printf( "\n" );
//    printf ( "--------------------%s\n", str_in );
    LOG_INFO << "--------------------" << str_in;
}

_tag_OutputDbgInfo::~_tag_OutputDbgInfo( ) {
//    printf ( "--------------------%s\n", m_str_out.c_str() );
//    printf( "\n" );
    LOG_INFO << "--------------------" << m_str_out;
    printf( "\n" );
}

//

int CurTid() {
    return CurrentThread::tid();
}

/*
    * Parameter(s)  :
    * Return        : true;false
    * Description   : 剔除字符串两端指定字符
*/
bool stringTrimEx( std::string& sInput, char c ) {
    size_t fst = sInput.find_first_not_of( c );
    size_t lst = sInput.find_last_not_of( c );

    if ( fst != std::string::npos )
        sInput = sInput.substr( fst, lst - fst + 1 );

    auto it = 1;
    return true;
}

//	转换为大写
void        MakeUpper( std::string& sInput ) {
    std::transform( sInput.begin(), sInput.end(), sInput.begin(), (int (*)(int))std::toupper ) ;
}
//	转换为小写
void        MakeLower( std::string& sInput ) {
    std::transform( sInput.begin(), sInput.end(), sInput.begin(), (int (*)(int))std::tolower );
}



bool isFileExist(const char* lpszfile) {
    return !access(lpszfile, F_OK);
//    std::string sFile = lpszfile;
//    fs::path fsFile( sFile );
//    return fs::exists(fsFile);
}

bool isFileExist(const std::string& infile) {
    return isFileExist(infile.c_str());
}

std::string GetCurrentDir() {
    char tmppath[2048] = {0};
    getcwd(tmppath,2048);
//    printf("%s",file_path_getcwd);
    return std::string(tmppath);
//    return boost::filesystem::initial_path<boost::filesystem::path>().string();
}


bool isPathExist(const char* lpszdir) {
    DIR *dp;
    if ((dp = opendir(lpszdir)) == NULL) {
        return false;
    }
    closedir(dp);
    return true;
    // boost::filesystem::path path_dir(lpszdir);
    // return boost::filesystem::exists(path_dir);
}
bool isPathExist(const std::string& indir) {
    return isPathExist(indir.c_str());
}


int mkpath(std::string s,mode_t mode=0755)
{
    size_t pre=0,pos;
    std::string dir;
    int mdret;

    if ( s[s.size()-1] != '/' ) {
        // force trailing / so we can handle everything in loop
        s += '/';
    }

    while((pos=s.find_first_of('/',pre))!=std::string::npos){
        dir=s.substr(0,pos++);
        pre=pos;
        if(dir.size()==0) continue; // if leading / first time is 0 length
        if((mdret=::mkdir(dir.c_str(),mode)) && errno!=EEXIST){
            return mdret;
        }
    }
    return mdret;
}

bool CreateFullDir(const char* lpszdir) {
    if (isPathExist(lpszdir)) {
        return true;
    } else {
//        printf( "lpszdir=%s not found, try to create dir\n", lpszdir );
        return mkpath(lpszdir) ? "false" : "true";
//        boost::filesystem::path path_dir(lpszdir);
//        return boost::filesystem::create_directories(path_dir);
    }
}

bool CreateFullDir(const std::string& indir) {
    return CreateFullDir(indir.c_str());
}

bool CopyFileEx( const char* lpszSrc, const char* lpszDst ) {

    std::string srcFileContent = GetFileContent_string(lpszSrc);
    if (srcFileContent.empty()) {
        LOG_ERROR << "srcFile content is null, srcFile=" << lpszSrc;
        return false;
    }

    return save_file(lpszDst, srcFileContent);
}

//bool CopyFileEx( const char* lpszSrc, const char* lpszDst ) {
//    std::ifstream in;
//    in.open(lpszSrc);
//    if (!in) {
////        LOG_ERROR( "open src file failed, file=" << lpszSrc );
//        return false;
//    }

//    std::ofstream out;
//    out.open(lpszDst);
//    if (!out) {
////        LOG_ERROR( "create new file failed, file=" << lpszDst );
//        in.close();
//        return false;
//    }

//    out << in.rdbuf();

//    out.close();
//    in.close();
//    return true;

////    boost::system::error_code ec;
////    try
////    {
////        boost::filesystem::copy_file( lpszSrc, lpszDst, boost::filesystem::copy_option::overwrite_if_exists, ec);
////    }
////    catch (const boost::filesystem::filesystem_error& e)
////    {
////        char tmpbuffer[ 4096 ] = {0};
////        sprintf( tmpbuffer, "copy_file Error=%s srcfile=%s dstfile=%s", e.what(), lpszSrc, lpszDst );
//// //        printf( tmpbuffer );
////        LOG_ERROR( tmpbuffer );
////    }
//}
bool CopyFileEx( const std::string& srcfile, const std::string& dstfile ) {
    return CopyFileEx( srcfile.c_str(), dstfile.c_str() );
}

bool save_file( const char* lpfilename, const std::string& sinput ) {
    FILE* f = fopen( lpfilename, "wb" );
    if ( !f ) {
        LOG_ERROR << "fopen write file: " << lpfilename << " error";
        return false;
    }
    fwrite(sinput.c_str(), sinput.size(), 1, f);
    fclose(f);
    return true;
}

bool save_file( const char* lpfilename, char* inbuffer, size_t insize ) {
    FILE* f = fopen( lpfilename, "wb" );
    if ( !f ) {
//        cout << "fopen write file: " << lpfilename << " error" << endl ;
        LOG_ERROR << "fopen write file: " << lpfilename << " error";
        return false;
    }
    fwrite( inbuffer, insize, 1, f);
    fclose(f);
    return true;
}



bool get_string_value(const rapidjson::Document& d, const char* name, std::string& value) {
    if (!d.IsObject()) {
        value.clear();
        printf("d is not Object !!!!");
//        LOG_DEBUG("d is not Object !!!!");
        return false;
    }
    if (d.HasMember(name)) {
        if (d[name].IsString()) {
            value = d[name].GetString();
            return true;
        }
    }
    value.clear();
    return false;
}




std::string SplitString( const std::string& s, std::set<std::string>& v, const char* lpszsep ) {
    std::string inSep = lpszsep;
    return SplitString( s, v, inSep );
}

// 使用 set 集合，去掉重复，索引term
std::string SplitString( const std::string& s, std::set<std::string>& v, const std::string& c ) {
    std::string sret_terms;

    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    if ( std::string::npos == pos2 ) {
        v.insert(s);
        return s;
    }

    pos1 = 0;
    while ( std::string::npos != pos2 ) {
        std::string str_tmp = s.substr(pos1, pos2-pos1);
        v.insert( str_tmp );

        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if ( pos1 != s.length() ) {
        std::string str_tmp = s.substr(pos1);
        v.insert( str_tmp ) ;
    }
    for ( std::set<std::string>::const_iterator cit = v.begin(); cit != v.end(); ++ cit ) {
        sret_terms += *cit ;
        sret_terms += " " ;
    }
    stringTrimEx( sret_terms );
    return sret_terms;
}



std::string GetFileContent_string( const std::string& inFileName ) {
    return GetFileContent_string(inFileName.c_str());
}
std::string GetFileContent_string( const char* lpszFileName ) {
    unsigned int fileSize = 0;
    char* pContent = GetSmallFileContent(lpszFileName, &fileSize);
    if (pContent) {
//        printf("123\n");
        std::string retFile(pContent, fileSize);
//        printf("retFileLen=%d\n", retFile.length());
        free(pContent);
        return retFile;
    } else {
        return "";
    }
}
char* GetSmallFileContent( const char* lpszFileName, unsigned int* pFileSize ) {
    FILE* pInfile = fopen( lpszFileName, "rb" );
    if ( pInfile == NULL ) {
        LOG_ERROR << "open infile failed, file=" << lpszFileName;
        return NULL;
    }

    fseek( pInfile, 0, SEEK_END );
    int inFileLen = ftell( pInfile );
    fseek( pInfile, 0, SEEK_SET );

    if ( pFileSize )
        *pFileSize = (unsigned int)inFileLen;

    char* pContent = (char*)malloc( inFileLen + 1 ); memset( pContent, 0, inFileLen + 1 );
    fread( pContent, sizeof(char), inFileLen, pInfile );
    fclose( pInfile );
//    std::cout << "filesize=" << inFileLen <<std::endl;
    return pContent;
}

unsigned int GetFileSize( const char* lpszFileName ) {
    FILE* pInfile = fopen( lpszFileName, "rb" );
    if ( pInfile == NULL ) {
        LOG_ERROR << "open infile failed, file=" << lpszFileName;
        return 0;
    }

    fseek( pInfile, 0, SEEK_END );
    unsigned int inFileLen = ftell( pInfile );
    fseek( pInfile, 0, SEEK_SET );
    fclose( pInfile );
    return inFileLen;
}


// log4cplus.appender.R.File=./logs/info.log
// SpecifyLine: log4cplus.appender.R.File=.
// newline: log4cplus.appender.R.File=g_strCaRoot/logs/info.log
void ReplaceLineInFile(const char *fileName, const char* SpecifyLine, const char *newline)
{
    std::ifstream in(fileName);
    if (!in) {
        LOG_ERROR << "openfile failed, file=" << fileName;
        return;
    }

    char line[4096] = {0};
    int  i = 0;
    std::string tempStr;
    while (in.getline(line, sizeof(line)))
    {
        i++;
        if ( strstr(line, SpecifyLine) == NULL ) {
            tempStr += line;
        }
        else {
            char* pPtr = line + strlen(SpecifyLine);
            tempStr += newline;
            tempStr += pPtr;

//            printf( "tempStr=%s\n", tempStr.c_str() );
        }
        tempStr += '\n';
    }
    in.close();

    std::ofstream out(fileName);
    if (!out) {
        LOG_ERROR << "writefile failed, file=" << fileName;
        return;
    }

    out<<tempStr;
    out.flush();
    out.close();
}


// 提取json内 字符串 内容
void GetStringFromJson( const rapidjson::Document& jsonReader, const char* szName, std::string& outString, const char* lpszDefault ) {
    if ( !jsonReader.HasMember(szName) ) {
        LOG_WARN << "no have item=" << szName;
        outString = lpszDefault;
        return;
    }
    if ( jsonReader[szName].IsString() ) {
        outString = jsonReader[ szName ].GetString();
    }
}
void GetInt64FromJson( const rapidjson::Document& jsonReader, const char* szName, int64_t& outVal, int64_t inDefault ) {
    if ( !jsonReader.HasMember(szName) ) {
        LOG_WARN << "no have item=" << szName;
        outVal = inDefault;
        return;
    }
    if ( jsonReader[szName].IsInt64() ) {
        outVal = jsonReader[ szName ].GetInt64();
    }
}
void GetIntFromJson( const rapidjson::Document& jsonReader, const char* szName, int32_t& outVal, int32_t inDefault ) {
    if ( !jsonReader.HasMember(szName) ) {
        LOG_WARN << "no have item=" << szName;
        outVal = inDefault;
        return;
    }
    if ( jsonReader[szName].IsInt() ) {
        outVal = jsonReader[ szName ].GetInt();
    }
}

//uint64_t stringToUint64_t(const char* lpszVal) {
//    return strtoull(lpszVal, NULL, 0);
//}

//bool stringToU64(const std::string& str, uint64_t& val)
//{
//    unsigned long long temp_val = 0;
//    bool isOK = strtoull(lpszVal, NULL, 0);
//    val = temp_val;
//    return isOK && (temp_val <= 0xffffffffffffffff/*64bit无符号整形的有效范围*/);
//}



void checktime::dump(const std::string& name, const std::string& _info, const std::string& info, const int32_t _boundary, int32_t diff) {
//    if (diff >= _boundary) {
//        LOG_WARN("--->>>"<<name<<" - "<<_info<<" COST:"<<diff<<" ms "<<info);
//    } else {
//        LOG_INFO_NO_FUNCTION(name << "?cost=" << diff << "ms");
//    }
    LOG_INFO << name << "?cost=" << diff << "ms";
}



uint64_t GetCurMilliSecond() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
//    printf("second:%ld\n",tv.tv_sec);  //秒
//    printf("millisecond:%ld\n",tv.tv_sec*1000 + tv.tv_usec/1000);  //毫秒
    return (tv.tv_sec*1000 + tv.tv_usec/1000);
}
std::string GetCurMilliSecond_string() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
//    printf("second:%ld\n",tv.tv_sec);  //秒
//    printf("millisecond:%ld\n",tv.tv_sec*1000 + tv.tv_usec/1000);  //毫秒

    std::ostringstream oret;
    oret << (tv.tv_sec*1000 + tv.tv_usec/1000);
    return oret.str();
}

int64_t GetRandom64() {

//    independent_bits_engine<default_random_engine,64,unsigned long long int> engine;
//    engine.seed(time(NULL));//设定随机数种子
//    return engine();
    return 0;
}





