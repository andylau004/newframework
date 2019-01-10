


#include "skipchars.h"

#include <locale>
#include <cstdlib>




static const char skip[] = " ";

void SkipChars::initSkipChars() {
    if (!skips_.empty()) {
        return;
    }

    std::setlocale(LC_ALL, "en_US.utf8");

    std::size_t size = sizeof(skip) / sizeof(skip[0]);

    wchar_t wstr[size+1];
    std::size_t n = std::mbstowcs(wstr, skip, size+1);

    for (std::size_t i = 0; i < n; ++i) {
        skips_.insert(wstr[i]);
    }
}

bool SkipChars::isSkipChars(wchar_t c) {
    if (skips_.empty())
        return false;
    return skips_.find(c) != skips_.end();
}

std::set<wchar_t> SkipChars::skips_;


