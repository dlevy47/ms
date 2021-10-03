#ifndef WZ_WZ_INTERNAL_H
#define WZ_WZ_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>

#include "wz/wz.h"

static const struct error _NO_ERROR = {0};

extern const uint8_t wz_key[];

int _wz_openfileforread(
        int* handle_out,
        size_t* size_out,
        const char* filename);

int _wz_closefile(
        int handle);

int _wz_mapfile(
        void** addr_out,
        int handle,
        size_t length);

int _wz_unmapfile(
        void* addr,
        size_t length);

#define CHECK_OFFSET(f, off) do { \
    if (off > (f->file_addr + f->file_size)) { \
        struct error err = error_new(ERROR_KIND_BADOFFSET, L"offset goes past end of file"); \
        error_top(&err)->bad_offset.at = off; \
        return err; \
    } \
} while (0)

// _lenstr populates a fixed-width string buffer with bytes starting at
// the specified offset, incrementing it along the way.
static inline struct error _lenstr_at(const struct wz* f, char* out, uint8_t** off, size_t len) {
    struct error err = {0};
    for (size_t i = 0; i < len; ++i) {
        *out = (char) **off;
        *off += 1;
        ++out;
        CHECK_OFFSET(f, *off);
    }

    return err;
}

// _nullstr returns a pointer to the null string at the specified offset,
// and increments the offset to point past the null pointer.
static inline struct error _nullstr_at(const struct wz* f, char** ret_out, uint8_t** off) {
    struct error err = {0};
    char* ret = (char*) *off;
    while (**off) {
        *off += 1;
        CHECK_OFFSET(f, *off);
    }
    *ret_out = ret;

    return err;
}

// _uint64_t_at reads the uint64_t at the specified offset, and increments
// the offset.
static inline struct error _uint64_t_at(
        const struct wz* f,
        uint64_t* value_out,
        uint8_t** off) {
    struct error err = {0};
    uint64_t value = *(uint64_t*) *off;
    *off += 8;
    CHECK_OFFSET(f, *off);
    *value_out = value;

    return err;
}

// _float_at reads the float at the specified offset, and increments
// the offset.
static inline struct error _float_at(
        const struct wz* f,
        float* value_out,
        uint8_t** off) {
    struct error err = {0};
    float value = *(float*) *off;
    *off += 4;
    CHECK_OFFSET(f, *off);
    *value_out = value;

    return err;
}

// _double_at reads the double at the specified offset, and increments
// the offset.
static inline struct error _double_at(
        const struct wz* f,
        double* value_out,
        uint8_t** off) {
    struct error err = {0};
    double value = *(double*) *off;
    *off += 8;
    CHECK_OFFSET(f, *off);
    *value_out = value;

    return err;
}

// _int32_t_at reads the int32_t at the specified offset, and increments
// the offset.
static inline struct error _int32_t_at(
        const struct wz* f,
        int32_t* value_out,
        uint8_t** off) {
    struct error err = {0};
    int32_t value = *(int32_t*) *off;
    *off += 4;
    CHECK_OFFSET(f, *off);
    *value_out = value;

    return err;
}

// _uint32_t_at reads the uint32_t at the specified offset, and increments
// the offset.
static inline struct error _uint32_t_at(
        const struct wz* f,
        uint32_t* value_out,
        uint8_t** off) {
    struct error err = {0};
    uint32_t value = *(uint32_t*) *off;
    *off += 4;
    CHECK_OFFSET(f, *off);
    *value_out = value;

    return err;
}

// _uint16_t_at reads the uint16_t at the specified offset, and increments
// the offset.
static inline struct error _uint16_t_at(
        const struct wz* f,
        uint16_t* value_out,
        uint8_t** off) {
    struct error err = {0};
    uint16_t value = *(uint16_t*) *off;
    *off += 2;
    CHECK_OFFSET(f, *off);
    *value_out = value;

    return err;
}

// _uint8_t_at reads the uint8_t at the specified offset, and increments
// the offset.
static inline struct error _uint8_t_at(
        const struct wz* f,
        uint8_t* value_out,
        uint8_t** off) {
    struct error err = {0};
    *value_out = **off;
    *off += 1;
    CHECK_OFFSET(f, *off);

    return err;
}

// _compressedint_at reads an uint32_t at the specified offset, in the low-value
// compressed format. The offset is incremented along the way.
static inline struct error _compressedint_at(
        const struct wz* f,
        uint32_t* value_out,
        uint8_t** off) {
    struct error err = {0};
    uint8_t first = 0;
    CHECK(_uint8_t_at(f, &first, off),
        ERROR_KIND_BADREAD, L"failed to read first byte of compressed int");
    if ((int8_t) first != -128) {
        *value_out = (uint32_t) first;
        return err;
    }

    uint32_t value = 0;
    CHECK(_uint32_t_at(f, &value, off),
        ERROR_KIND_BADREAD, L"failed to read compressed int");
    *value_out = value;
    return err;
}

// _offset_at reads a 4 byte offset value from the specified offset. The specified
// offset is incremented along the way.
static inline struct error _offset_at(
        const struct wz* f,
        uint32_t* offset_out,
        uint8_t** off) {
    struct error err = {0};
    uint32_t value = (*off - f->file_addr - f->header.file_start) ^ 0xFFFFFFFF;
    value *= f->header.version_hash;
    value -= 0x581C3F6D;
    value = (value << (value & 0x1F)) | (value >> (32 - value & 0x1F));

    uint32_t xor = 0;
    CHECK(_uint32_t_at(f, &xor, off),
        ERROR_KIND_BADREAD, L"failed to read xor");
    value ^= xor;
    value += f->header.file_start * 2;
    *offset_out = value;
    return err;
}

#endif
