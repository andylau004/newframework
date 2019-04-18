#ifndef BADDWORD_CONVERT_H_
#define BADDWORD_CONVERT_H_

#include <string>
#include <stdint.h>

//const int32_t OPER_DELETEWORD = 2;
//const int32_t OPER_ADDWORD = 1;

//struct WordOper {
//    std::string word;
//    int oper;
//    WordOper():word(""),oper(0) {}
//};

//template <typename out, class in>
//out convert(const in & value) {
//    std::stringstream stream;
//    stream << value;
//    out r;
//    stream >> r;
//    return r;
//}


enum Encoding
{
    kUnknown, kUnicode, kGBK, kUTF8,
};


class Convert {
public:
    ~Convert() {}
    static std::string convert(const std::wstring& wstr);
    static std::wstring convert(const std::string& str);

    static const char* getEncodingName(Encoding enc);
    static const char* getEncodingName(const std::string& content);

    static int countChineseCharsUtf8(const std::string& content);
    static Encoding detectEncoding(const std::string& content);
    static int countChineseCharsGbk(const std::string& content);
    static int countChineseCharsUcs2(const std::string& content);

private:
    Convert() {}
};


//// convert<int8_t> 获取到ASCII码
//template <typename out, class in>
//out convert(const in & value) {
//    std::stringstream stream;
//    stream << value;
//    out r;
//    stream >> r;
//    return r;
//}




#endif  // BADDWORD_CONVERT_H_

