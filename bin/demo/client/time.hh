#pragma once

#include <cmath>
#include <cstdint>

namespace client {

struct Time {
    int64_t seconds;
    int64_t microseconds;

    float to_float() const {
        return
            static_cast<float>(seconds) + (
                    static_cast<float>(microseconds) / 1000000);
    }
    double to_double() const {
        return
            static_cast<double>(seconds) + (
                    static_cast<double>(microseconds) / 1000000);
    }

    uint64_t to_milliseconds() const {
        return static_cast<uint64_t>(seconds) * 1000 + static_cast<uint64_t>(microseconds / 1000);
    }

    template <typename T>
    static Time from(T val) {
        Time t = {
            .seconds = static_cast<int64_t>(val),
            .microseconds = static_cast<int64_t>((val - std::floor(val)) * 1000000),
        };

        return t;
    }

    Time operator-(const Time& rhs) const {
        return Time::from(to_double() - rhs.to_double());
    }
};

Time now();

struct Timer {
    Time start;

    Time since(Time now) const {
        return now - start;
    }
};

}
