#pragma once

#include <cstdint>
#include <string>

#include "util/error.hh"
#include "wz/extents.hh"

namespace wz {

struct Wz;

struct Parser {
    uint8_t* address;
    const Wz* wz;

    Error i8(int8_t* x) {return primitive(x, &address);}
    Error u8(uint8_t* x) {return primitive(x, &address);}
    Error u16(uint16_t* x) {return primitive(x, &address);}
    Error i32(int32_t* x) {return primitive(x, &address);}
    Error u32(uint32_t* x) {return primitive(x, &address);}
    Error u64(uint64_t* x) {return primitive(x, &address);}
    Error f32(float* x) {return primitive(x, &address);}
    Error f64(double* x) {return primitive(x, &address);}

    Error wstring_fixedlength(
            std::wstring* s,
            size_t len_bytes);
    Error wstring_nullterminated(
            std::wstring* s);

    Error i32_compressed(int32_t* x);
    Error offset(uint32_t* x);

    template<typename T>
        Error primitive(
                T* x,
                uint8_t** address);
};

}
