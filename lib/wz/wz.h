#ifndef WZ_WZ_H
#define WZ_WZ_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "util/error.h"
#include "wz/property.h"

struct wz;

struct wz_file {
    uint8_t* base;
    struct wz_propertycontainer root;
};

struct wz_directory {
    uint32_t count;
    uint8_t* first_entry;
};

// wz_directory_from populates the directory from offset, incrementing
// it along the way.
struct error wz_directory_from(
        const struct wz* f,
        struct wz_directory* d,
        uint8_t** off);

enum wz_directoryentry_kind {
    WZ_DIRECTORYENTRY_KIND_UNSPECIFIED,
    WZ_DIRECTORYENTRY_KIND_UNKNOWN,
    WZ_DIRECTORYENTRY_KIND_FILE,
    WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY,
};

static inline const char* wz_directoryentry_kind_str(
        enum wz_directoryentry_kind t) {
    switch (t) {
        case WZ_DIRECTORYENTRY_KIND_UNKNOWN:
            return "WZ_DIRECTORYENTRY_KIND_UNKNOWN";
        case WZ_DIRECTORYENTRY_KIND_FILE:
            return "WZ_DIRECTORYENTRY_KIND_FILE";
        case WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY:
            return "WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY";
        case WZ_DIRECTORYENTRY_KIND_UNSPECIFIED:
        default:
            return "WZ_DIRECTORYENTRY_KIND_??";
    }
}

struct wz_directoryentry {
    enum wz_directoryentry_kind kind;
    union {
        struct {
            uint32_t unknown1;
            uint16_t unknown2;
            uint32_t offset;
        } unknown;

        struct {
            struct wz_encryptedstring name;
            uint32_t size;
            uint32_t checksum;
            struct wz_directory subdirectory;
        } subdirectory;

        struct {
            struct wz_encryptedstring name;
            uint32_t size;
            uint32_t checksum;
            struct wz_file file;
        } file;
    };
};

// wz_directoryentry_from populates the directory entry from offset,
// incrementing it along the way.
struct error wz_directoryentry_from(
        const struct wz* f,
        struct wz_directoryentry* d,
        uint8_t** off);

struct wz_header {
    // ident is a 4-character ident string, with space for a null included.
    char ident[5];
    uint64_t file_size;
    uint32_t file_start;
    char* copyright;
    uint16_t version;
    uint32_t version_hash;
};

struct wz {
    struct wz_header header;
    uint16_t version;

    int fd;
    size_t file_size;
    uint8_t* file_addr;

    struct wz_directory root;
};

// wz_open opens a wz file at the specified path and populates the header field.
struct error wz_open(
        struct wz* f,
        const char* filename);

// wz_openfrom opens a wz file from memory at the specified address.
struct error wz_openfrom(
        struct wz* f,
        void* addr);

// wz_close closes a wz file.
struct error wz_close(
        struct wz* f);

#endif
