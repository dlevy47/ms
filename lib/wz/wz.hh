// wz contains a library for one-copy, zero-allocation reading and parsing of WZ files.
#pragma once

#include "p.hh"
#include "util/error.hh"
#include "wz/directory.hh"
#include "wz/parser.hh"

namespace wz {

// Header contains data in a WZ file header.
struct Header {
    // ident is the magic characters for the file. Usually "PKG1".
    std::wstring ident;

    // file_size is the declared size of the WZ file data. Unused.
    uint64_t file_size;

    // file_start is the declared offset of the WZ file data. Unused.
    uint32_t file_start;

    // copyright is a null-terminated copyright string for the file. For human
    // presentation only.
    std::wstring copyright;

    // version is the obfuscated version number of the WZ file.
    uint16_t version;

    // version_hash is the hash of the computed un-obfuscated version number.
    uint32_t version_hash;

    // parse reads header data from a WZ parser.
    static Error parse(
            Header* h,
            Parser* p);
};

// Wz is a no-allocation lazy WZ file reader. WZ files are read from memory
// (either via mapping the file into memory, or some other way).
struct Wz {
    // header is the materialized header information of the wz file.
    Header header;

    // version is the computed un-obfuscated version of the data contained in the wz file.
    N<uint16_t> version;

    // fd is the OS file descriptor of the opened wz file, if owned by this Wz.
    N<int> fd;

    // file is the memory extents of the opened wz file, once mapped into memory.
    Extents file;

    // root is the root wz directory that contains all the data in the file.
    Directory root;

    // Wz has a non-trivial destructor that calls `close`.
    ~Wz();

    // open maps a WZ file into memory, and parses the initial root directory.
    static Error open(
            Wz* wz,
            const char* filename);

    // parse parses a WZ file from a memory range. To use parse, first set
    // `file` with the address range of the WZ file contents in memory.
    static Error parse(
            Wz* wz,
            Parser* p);

    // close unmaps a WZ file, if it was mapped by this Wz.
    Error close();

    Wz() = default;
    Wz(Wz&&) = default;
    Wz(const Wz&) = delete;
};

}
