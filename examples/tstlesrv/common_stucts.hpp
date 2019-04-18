

#pragma once



#include <stdio.h>
#include <string>

#include "muduo/base/common.h"


typedef struct Item_s {
    Item_s():m_port(0),m_socket(INVALID_SOCKET){}
    int         m_port;
    SOCKET      m_socket;
    std::string m_ip;
    std::string m_type_ip;
} Item_t;

union ClientNotifyInfo {
    char buf[sizeof(int64_t)];
    int64_t sessionID;
};


struct CRTValidateTime {
    std::string validityTime;
};

struct RefreshCRTRst {
    int32_t code;
    std::string msg;
    CRTValidateTime result;
    RefreshCRTRst() : code(-1) {

    }
};




