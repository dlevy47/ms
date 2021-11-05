#pragma once

#include "util/error.hh"
#include "wz/directory.hh"
#include "wz/parser.hh"

namespace wz {

struct Header {
    std::wstring ident;
    uint64_t file_size;
    uint32_t file_start;
    std::wstring copyright;
    uint16_t version;
    uint32_t version_hash;

    static Error parse(
            Header* h,
            Parser* p);
};

struct Wz {
    Header header;
    uint16_t version;

    int fd;
    Extents file;

    Directory root;

    Wz() {}
    ~Wz();

    static Error open(
            Wz* wz,
            const char* filename);
    static Error parse(
            Wz* wz,
            Parser* p);
    Error close();

    Wz(const Wz&) = delete;
};

}
