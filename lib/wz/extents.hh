#pragma once

#include <cstdint>
#include <iostream>

namespace wz {

struct Extents {
    const uint8_t* start;
    const uint8_t* end;

    bool valid(const uint8_t* at, size_t count) const {
        return at >= start && (at + count) <= end;
    }
};

template <typename CharT>
std::basic_ostream<CharT>& operator<<(std::basic_ostream<CharT>& s, const Extents& e) {
    auto state = s.flags();
    s << std::hex << "[0x" << e.start << ", 0x" << e.end << "]";
    s.flags(state);
    return s;
}

}
