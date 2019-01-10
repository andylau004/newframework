

#pragma once



#include <set>

class SkipChars {
public:
    ~SkipChars() {}
    static void initSkipChars();
    static bool isSkipChars(wchar_t c);
private:
    SkipChars() {}
    static std::set<wchar_t> skips_;
};



