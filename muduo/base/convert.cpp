#include "convert.h"
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <assert.h>


std::string Convert::convert(const std::wstring& wstr) {
    size_t size = wstr.size();
    if (size == 0) {
        return "";
    }
    size_t cs = size * sizeof(wchar_t) + 1;
    char c[cs];
    memset(c, '\0', cs);

    int n = std::wcstombs(c, wstr.c_str(), cs);
    if (n > 0)
        return std::string(c, n);
    return "";
}

std::wstring Convert::convert(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    int size = str.size();
    wchar_t wchar[size+1];
    memset(wchar, '\0', sizeof(wchar_t) * (size+1));
    int n = std::mbstowcs(wchar, str.c_str(), size+1);
    if (n <= 0) {
        return std::wstring();
    }

    std::wstring wstr (wchar, n);
    //std::cout << n << " : size :  " << wstr.size() << " word:" << str << "\n";
    return wstr;
}





const char* Convert::getEncodingName(Encoding enc)
{
    if (enc == kUTF8) {
        return "UTF-8";
    } else if (enc == kUnicode) {
        return "Unicode";
    } else {
        return "Unknown";
    }
}
const char* Convert::getEncodingName(const std::string& content) {
    return getEncodingName(detectEncoding(content));
}

Encoding Convert::detectEncoding(const std::string& content)
{
    if (content.size() < 2)
        return kGBK;

    if (content[0] == '\xFF' && content[1] == '\xFE') {
        return kUnicode;
    } else {
        for (size_t i = 0; i < content.size()-2;) {
            if (content[i] & 0x80) {
                if ((content[i+1] & 0x80) == 0) {
                    return kGBK;
                } else if ((content[i] & 0xE0) == 0xC0) {
                    if ((content[i+1] & 0xC0) == 0x80) {
                        return kGBK;
                    } else {
                        i += 2;
                    }
                } else if ((content[i] & 0xF0) == 0xE0) {
                    if ((content[i+1] & 0xC0) == 0x80) {
                        if ((content[i+2] & 0xC0) == 0x80) {
                            //printf("UTF-8 !!!\n");
                            return kUTF8;
                        } else {
                            return kGBK;
                        }
                    } else {
                        return kGBK;
                    }
                } else {
                    return kGBK;
                }
            } else {
                ++i;
            }
        }
        return kUTF8;
    }
}

int Convert::countChineseCharsGbk(const std::string& content)
{
    int cnt = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] & 0x80) {
            ++cnt;
            ++i;
        }
    }
    return cnt;
}

int Convert::countChineseCharsUcs2(const std::string& content)
{
    assert(content.size() % 2 == 0);
    int cnt = 0;
    const uint16_t* p = reinterpret_cast<const uint16_t*>(content.c_str());
    ++p;
    for (size_t i = 2; i < content.size()/2; ++i) {
        if (*p++ > 127)
            ++cnt;
    }
    return cnt;
}

int Convert::countChineseCharsUtf8(const std::string& content)
{
    int cnt = 0;
    for (size_t i = 0; i < content.size();) {
        char c = content[i];
        if ((c & 0x80) == 0) {
            ++i;
        } else if ((c & 0xe0) == 0xc0) {
            i += 2;
        } else if ((c & 0xf0) == 0xe0) {
            i += 3;
            ++cnt;
        } else {
            abort();
        }
    }
    return cnt;
}
