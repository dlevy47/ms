#include "wz/parser.hh"

#include "wz/wz.hh"

namespace wz {

template<typename T>
Error Parser::primitive(
    T* x,
    const uint8_t** address) {
    if (!wz->file.valid(*address, sizeof(T)))
        return error_new(Error::INVALIDOFFSET)
        << "address 0x" << std::hex << *address
        << " is outside of file extents " << wz->file;

    *x = *reinterpret_cast<const T*>(*address);
    *address += sizeof(T);

    return Error();
}

template Error Parser::primitive<int8_t>(int8_t* x, const uint8_t** address);
template Error Parser::primitive<uint8_t>(uint8_t* x, const uint8_t** address);
template Error Parser::primitive<uint16_t>(uint16_t* x, const uint8_t** address);
template Error Parser::primitive<int32_t>(int32_t* x, const uint8_t** address);
template Error Parser::primitive<uint32_t>(uint32_t* x, const uint8_t** address);
template Error Parser::primitive<uint64_t>(uint64_t* x, const uint8_t** address);
template Error Parser::primitive<float>(float* x, const uint8_t** address);
template Error Parser::primitive<double>(double* x, const uint8_t** address);

Error Parser::wstring_fixedlength(
    std::wstring* s,
    size_t len_bytes) {
    if (!wz->file.valid(address, len_bytes))
        return error_new(Error::INVALIDOFFSET);

    s->reserve(len_bytes);
    for (size_t i = 0; i < len_bytes; ++i) {
        s->push_back(static_cast<wchar_t>(*address));
        ++address;
    }

    return Error();
}

Error Parser::wstring_nullterminated(
    std::wstring* s) {
    size_t i = 0;
    do {
        if (!wz->file.valid(address, 1))
            return error_new(Error::INVALIDOFFSET)
            << "string index " << i << " is out of bounds";

        wchar_t c = static_cast<wchar_t>(*address);
        ++address;
        if (c == 0) break;

        s->push_back(c);
    } while (true);

    return Error();
}

Error Parser::i32_compressed(int32_t* x) {
    int8_t first = 0;
    CHECK(i8(&first), Error::BADREAD);
    if (first != -128) {
        *x = static_cast<int32_t>(first);
        return Error();
    }

    return i32(x);
}

Error Parser::offset(uint32_t* x) {
    uint32_t value = (address - wz->file.start - wz->header.file_start) ^ 0xFFFFFFFF;
    value *= wz->header.version_hash;
    value -= 0x581C3F6D;
    value = (value << (value & 0x1F)) | (value >> (32 - (value & 0x1F)));

    uint32_t xor_value = 0;
    CHECK(u32(&xor_value),
        Error::BADREAD) << "failed to read offset xor";
    value ^= xor_value;
    value += wz->header.file_start * 2;
    *x = value;

    return Error();
}

}
