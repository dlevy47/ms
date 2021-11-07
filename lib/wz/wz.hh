// wz contains a library for one-copy, zero-allocation reading and parsing of WZ files.
#pragma once

#include "p.hh"
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
    // header is the materialized header information of the wz file.
    Header header;

    // version is the version of the data contained in the wz file.
    N<uint16_t> version;

    // fd is the OS file descriptor of the opened wz file.
    N<int> fd;

    // file is the memory extents of the opened wz file, once mapped into memory.
    Extents file;

    // root is the root wz directory that contains all the data in the file.
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

    Wz(Wz&&) = default;
    Wz(const Wz&) = delete;
};

}
