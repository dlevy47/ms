#pragma once

#include <cstdint>
#include <iostream>

namespace wz {

// Extents represents a memory address range, and provides a helper method for
// bounds checking.
struct Extents {
    // start is the first valid memory address in the range.
    const uint8_t* start;

    // end is the last valid memory address in the range + 1. That is, the range
    // is exclusive.
    const uint8_t* end;

    // valid returns whether a read would stay in the bounds of the extent.
    bool valid(const uint8_t* at, size_t count) const {
        // The range is exclusive.
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
