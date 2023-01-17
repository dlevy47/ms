#include "client/time.hh"

#include <sys/time.h>

namespace client {

Time now() {
    struct timeval tv = { 0 };
    gettimeofday(&tv, nullptr);

    Time t = {
        .seconds = tv.tv_sec,
        .microseconds = tv.tv_usec,
    };
    return t;
}

}
