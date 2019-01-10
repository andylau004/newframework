



#include "codeconvert.h"


#include <sstream>
#include <iostream>

#include <cassert>
#include <stdio.h>

#include <string.h>


// codecvt_utf8_utf16 example
#include <iostream>
#include <locale>
#include <string>
//#include <codecvt>
#include <locale>



#include <iostream>       // std::cout, std::hex
#include <string>         // std::string, std::u32string
#include <locale>         // std::wstring_convert
//#include <codecvt>        // std::codecvt_utf8
#include <cstdint>        // std::uint_least32_t


#include <locale>
#include <cstdlib>



#include "skipchars.h"


using namespace std;


bool ras_is_text_utf8(const char* str,long length)
{
    int i;
    //UFT8可用1-6个字节编码,ASCII用一个字节
    int nBytes=0;
    unsigned char chr;
    //如果全部都是ASCII, 说明不是UTF-8
    bool bAllAscii = true;
    for(i=0;i<length;i++)
    {
        chr= *(str+i);
        // 判断是否ASCII编码,如果不是,说明有可能是UTF-8,ASCII用7位编码,但用一个字节存,最高位标记为0,o0xxxxxxx
        if( (chr&0x80) != 0 )
        {
            bAllAscii = false;
        }
        //如果不是ASCII码,应该是多字节符,计算字节数
        if(nBytes==0)
        {
            if(chr>=0x80)
            {
                if(chr>=0xFC&&chr<=0xFD)
                {
                    nBytes=6;
                }
                else if(chr>=0xF8)
                {
                    nBytes=5;
                }
                else if(chr>=0xF0)
                {
                    nBytes=4;
                }
                else if(chr>=0xE0)
                {
                    nBytes=3;
                }
                else if(chr>=0xC0)
                {
                    nBytes=2;
                }
                else
                {
                    return false;
                }
                nBytes--;
            }
        }
        //多字节符的非首字节,应为 10xxxxxx
        else
        {
            if( (chr&0xC0) != 0x80 )
            {
                return false;
            }
            nBytes--;
        }
    }
    //违返规则
    if( nBytes > 0 )
    {
        return false;
    }
    //如果全部都是ASCII, 也是字符串
    if( bAllAscii )
    {
        return true;
    }
    return true;
}



void tst_1() {

    std::string strCreateRemoteDir;
    strCreateRemoteDir += std::string(" \' mkdir -p /123/333 \' ");
    std::cout << "remotedir=" << strCreateRemoteDir.c_str() << std::endl;
    return ;


//    SkipChars::initSkipChars();
    std::setlocale(LC_ALL, "en_US.utf8");

    std::stringstream ottt;
    ottt << "中国N国屏蔽关键词";
    std::cout << "ottt=" << ottt.str().c_str() << std::endl;

    std::cout << "ottt code=" << ras_is_text_utf8( ottt.str().c_str(), ottt.str().size() ) << std::endl;

    {
//        {
//            std::stringstream oxx;
//            oxx << "中国";
//            size_t sizexx = oxx.str().size();
//            std::cout << "sizexx=" << sizexx << std::endl;
//        }
//        {
//            std::stringstream oxx;
//            oxx << "NBA";
//            size_t sizexx = oxx.str().size();
//            std::cout << "sizexx=" << sizexx << std::endl;
//        }

        const int fix_size = 8;

        size_t size = ottt.str().size();
        std::cout << "ottt size=" << size << std::endl;//9

        wchar_t wstr[fix_size + 1];
//        wchar_t wstr[ 128 ]; // = {0};
        std::cout << "sizeof(wstr)=" << sizeof(wstr) << std::endl;//40
        std::cout << "sizeof(wchar_t)=" << sizeof(wchar_t) << std::endl;//4
        std::cout << "sizeof(char)=" << sizeof(char) << std::endl;
        ::memset( wstr, 0, sizeof(wstr) );
//        return;

        size_t transferLen = std::mbstowcs(wstr, ottt.str().c_str(), fix_size);

        std::cout << "transferLen=" << transferLen << std::endl;//5
        std::cout << "wcslen(wstr)=" << wcslen(wstr) << std::endl;//5

        std::wcout << "wstr_ex=" << wstr << '\n';

//        transferLen = 8;
        char lstbuffer[ transferLen /** sizeof(wchar_t)*/ + 1 ];
        memset( lstbuffer, 0, transferLen /** sizeof(wchar_t)*/ + 1 );
        transferLen = std::wcstombs(lstbuffer, wstr, transferLen/* * sizeof(wchar_t)*/ );
        std::cout << "lst transferLen=" << transferLen << std::endl;//
        std::cout << "lstbuffer=" << lstbuffer << std::endl;//

    }
    return;

    std::u16string st1;

//    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
//    std::u16string dest = convert.from_bytes(ottt.str());

//    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(ottt.str());
    // UTF-8 to UTF-16/char16_t
//    std::u16string u16_conv = std::wstring_convert<
//        std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(ottt.str());
//    assert(u16 == u16_conv);
//    std::cout << "UTF-8 to UTF-16 conversion produced "
//              << u16_conv.size() << " code units:\n";
//    for (char16_t c : u16_conv)
//        std::cout << std::hex << std::showbase << c << ' ';
    return;


    std::ostringstream o_1 ;
    o_1 << 7777777 << std::endl;
    std::cout << "o_1=" << o_1.str().c_str();

    std::cout << "o_1 code=" << ras_is_text_utf8( o_1.str().c_str(), o_1.str().size() ) << std::endl;

}



/*
 *
 *
#include "Utils.hpp"

#include <string>
#include <locale>
#include <codecvt>

namespace ESTK_N {

std::u16string utf8_to_u16string(const std::string& str)
{
  const std::u16string u16_conv = std::wstring_convert<
        std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(str);
  return u16_conv;
}

std::string u16string_to_utf8(const std::u16string& str)
{
  const std::string u8_conv = std::wstring_convert<
        std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(str);
  return u8_conv;
}

}

*/




