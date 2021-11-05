#pragma once

#include <cstdint>

namespace wz {

struct Extents {
    uint8_t* start;
    uint8_t* end;

    bool valid(uint8_t* at, size_t count) const {
        return at >= start && (at + count) <= end;
    }
};

}
