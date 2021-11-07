#include "client/time.hh"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

namespace client {

// https://stackoverflow.com/questions/10905892
Time now() {
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    Time t = {
        .seconds = (long) ((time - EPOCH) / 10000000L),
        .microseconds = (long) (system_time.wMilliseconds * 1000),
    };
    return t;
}

}
