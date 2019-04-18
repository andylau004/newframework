#ifndef ONEPIECE_UTILS_TIMERUTIL_H_
#define ONEPIECE_UTILS_TIMERUTIL_H_

#include <stdio.h>
#include <stdlib.h>

#include <time.h>

class TimerUtils {
public:
    TimerUtils() {}
    ~TimerUtils() {}

    static int64_t get_current_nanotime() {
#ifdef __linux__
        timespec tp;
        int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
        if (ret == 0) {
            return tp.tv_sec * 1e9 + tp.tv_nsec;
        }
#endif
        return (int64_t)-1;
    }
private:
    TimerUtils(const TimerUtils&);
    TimerUtils& operator=(const TimerUtils&);
};

#endif  // ONEPIECE_UTILS_TIMERUTIL_H_
