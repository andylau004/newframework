//#include "evpp/inner_pre.h"

//#include "evpp/duration.h"
#include "duration.h"

//namespace evpp {
const int64_t Duration::kNanosecond = 1LL;
const int64_t Duration::kMicrosecond = 1000 * kNanosecond;
const int64_t Duration::kMillisecond = 1000 * kMicrosecond;
const int64_t Duration::kSecond = 1000 * kMillisecond;
const int64_t Duration::kMinute = 60 * kSecond;
const int64_t Duration::kHour = 60 * kMinute;
//}



//namespace evpp {
Duration::Duration()
    : ns_(0) {}

Duration::Duration(const struct timeval& t)
    : ns_(t.tv_sec * kSecond + t.tv_usec * kMicrosecond) {}

Duration::Duration(int64_t nanoseconds)
    : ns_(nanoseconds) {}

Duration::Duration(int nanoseconds)
    : ns_(nanoseconds) {}

Duration::Duration(double seconds)
    : ns_((int64_t)(seconds * kSecond)) {}

int64_t Duration::Nanoseconds() const {
    return ns_;
}

double Duration::Seconds() const {
    return double(ns_) / kSecond;
}

double Duration::Milliseconds() const {
    return double(ns_) / kMillisecond;
}

double Duration::Microseconds() const {
    return double(ns_) / kMicrosecond;
}

double Duration::Minutes() const {
    return double(ns_) / kMinute;
}

double Duration::Hours() const {
    return double(ns_) / kHour;
}

bool Duration::IsZero() const {
    return ns_ == 0;
}

struct timeval Duration::TimeVal() const {
    struct timeval t;
    To(&t);
    return t;
}

void Duration::To(struct timeval* t) const {
    t->tv_sec = (long)(ns_ / kSecond);
    t->tv_usec = (long)(ns_ % kSecond) / (long)kMicrosecond;
}

bool Duration::operator<(const Duration& rhs) const {
    return ns_ < rhs.ns_;
}

bool Duration::operator<=(const Duration& rhs) const {
    return ns_ <= rhs.ns_;
}

bool Duration::operator>(const Duration& rhs) const {
    return ns_ > rhs.ns_;
}

bool Duration::operator>=(const Duration& rhs) const {
    return ns_ >= rhs.ns_;
}

bool Duration::operator==(const Duration& rhs) const {
    return ns_ == rhs.ns_;
}

Duration Duration::operator+=(const Duration& rhs) {
    ns_ += rhs.ns_;
    return *this;
}

Duration Duration::operator-=(const Duration& rhs) {
    ns_ -= rhs.ns_;
    return *this;
}

Duration Duration::operator*=(int n) {
    ns_ *= n;
    return *this;
}

Duration Duration::operator/=(int n) {
    ns_ /= n;
    return *this;
}
//} // namespace evpp

