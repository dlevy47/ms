#ifndef WZ_WZ_H
#define WZ_WZ_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct wz;
struct wz_file;

enum wz_error_kind {
    WZ_ERROR_KIND_NONE,
    WZ_ERROR_KIND_BADOFFSET,
    WZ_ERROR_KIND_ERRNO,
    WZ_ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND,
    WZ_ERROR_KIND_FILEPARSEERROR1,
    WZ_ERROR_KIND_FILEPARSEERROR2,
    WZ_ERROR_KIND_FILEPARSEERROR3,
    WZ_ERROR_KIND_UNKNOWNSTRINGOFFSETKIND,
    WZ_ERROR_KIND_UNKNOWNPROPERTYKIND,
    WZ_ERROR_KIND_UNKNOWNPROPERTYKINDNAME,
    WZ_ERROR_KIND_UNKNOWNIMAGEFORMAT,
    WZ_ERROR_KIND_DECOMPRESSIONFAILED,
};

static inline const char* wz_error_kind_str(
        enum wz_error_kind k) {
    switch (k) {
        case WZ_ERROR_KIND_NONE:
            return "WZ_ERROR_KIND_NONE";
        case WZ_ERROR_KIND_BADOFFSET:
            return "WZ_ERROR_KIND_BADOFFSET";
        case WZ_ERROR_KIND_ERRNO:
            return "WZ_ERROR_KIND_ERRNO";
        case WZ_ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND:
            return "WZ_ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND";
        case WZ_ERROR_KIND_FILEPARSEERROR1:
            return "WZ_ERROR_KIND_FILEPARSEERROR1";
        case WZ_ERROR_KIND_FILEPARSEERROR2:
            return "WZ_ERROR_KIND_FILEPARSEERROR2";
        case WZ_ERROR_KIND_FILEPARSEERROR3:
            return "WZ_ERROR_KIND_FILEPARSEERROR3";
        case WZ_ERROR_KIND_UNKNOWNSTRINGOFFSETKIND:
            return "WZ_ERROR_KIND_UNKNOWNSTRINGOFFSETKIND";
        case WZ_ERROR_KIND_UNKNOWNPROPERTYKIND:
            return "WZ_ERROR_KIND_UNKNOWNPROPERTYKIND";
        case WZ_ERROR_KIND_UNKNOWNPROPERTYKINDNAME:
            return "WZ_ERROR_KIND_UNKNOWNPROPERTYKINDNAME";
        case WZ_ERROR_KIND_UNKNOWNIMAGEFORMAT:
            return "WZ_ERROR_KIND_UNKNOWNIMAGEFORMAT";
        case WZ_ERROR_KIND_DECOMPRESSIONFAILED:
            return "WZ_ERROR_KIND_DECOMPRESSIONFAILED";
        default:
            return "WZ_ERROR_KIND_??";
    }
}

struct wz_error {
    enum wz_error_kind kind;

    union {
        struct {
            uint8_t* from;
            uint8_t* at;
        } bad_offset;

        int errno_;

        uint8_t unknown_directoryentry_kind;
        uint8_t unknown_string_offset_kind;
        uint8_t unknown_property_kind;
        struct {
            uint32_t format;
            uint8_t format2;
        } unknown_image_format;

        int decompression_failed;
    };
};

static inline int wz_error_printto(const struct wz_error* e, FILE* outf) {
    int count = 0;
    int ret = 0;

    ret = fprintf(outf, "%s: ", wz_error_kind_str(e->kind));
    if (ret < 0) return ret;
    count += ret;

    switch (e->kind) {
        case WZ_ERROR_KIND_NONE:
            break;
        case WZ_ERROR_KIND_BADOFFSET:
            {
                ret = fprintf(outf, "from: %p at: %p", e->bad_offset.from, e->bad_offset.at);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_ERRNO:
            {
                ret = fprintf(outf, "errno %d: %s", e->errno_, strerror(e->errno_));
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND:
            {
                ret = fprintf(outf, "unknown directory entry kind %d", e->unknown_directoryentry_kind);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_FILEPARSEERROR1:
            {
                ret = fprintf(outf, "failed to parse file: mismatch name location");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_FILEPARSEERROR2:
            {
                ret = fprintf(outf, "failed to parse file: name mismatch");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_FILEPARSEERROR3:
            {
                ret = fprintf(outf, "failed to parse file: 3");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_UNKNOWNSTRINGOFFSETKIND:
            {
                ret = fprintf(outf, "unknown string offset kind: %d", e->unknown_string_offset_kind);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_UNKNOWNPROPERTYKIND:
            {
                ret = fprintf(outf, "unknown property kind: %d", e->unknown_property_kind);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_UNKNOWNPROPERTYKINDNAME:
            {
                ret = fprintf(outf, "unknown property kind name");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_UNKNOWNIMAGEFORMAT:
            {
                ret = fprintf(outf, "unknown image format: %u %u", e->unknown_image_format.format, e->unknown_image_format.format2);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case WZ_ERROR_KIND_DECOMPRESSIONFAILED:
            {
                ret = fprintf(outf, "zlib decompression failed: %d", e->decompression_failed);
                if (ret < 0) return ret;
                count += ret;
            } break;
    }

    return count;
}

enum wz_encryptedstring_kind {
    WZ_ENCRYPTEDSTRING_KIND_ONEBYTE,
    WZ_ENCRYPTEDSTRING_KIND_TWOBYTE,
};

struct wz_encryptedstring {
    enum wz_encryptedstring_kind kind;
    uint8_t* at;
    uint32_t len;
};

// wz_encryptedstring_decrypt decrypts the provided wz_encryptedstring into a
// provided buffer. The out buffer should be at least s->len wchar_t's in size.
struct wz_error wz_encryptedstring_decrypt(
        const struct wz* f,
        const struct wz_encryptedstring* s,
        wchar_t* out);

struct wz_propertycontainer {
    uint32_t count;
    uint8_t* first;
};

struct wz_namedpropertycontainer {
    uint32_t count;
    uint8_t* first;
};

struct wz_image {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint8_t format2;
    uint32_t unknown1;
    uint32_t length;
    uint8_t unknown2;

    uint8_t* data;
};

// wz_image_rawsize returns the uncompressed, unencrypted size of this image's data.
// Buffers passed to wz_image_pixels should be at least this size, in bytes.
uint32_t wz_image_rawsize(
        const struct wz_image* img);

// wz_image_pixels decodes the image data pointed to by img, and places the pixels
// into out, in RGBA8 format; that is: 1 byte r, 1 byte g, 1 byte b, 1 byte a.
struct wz_error wz_image_pixels(
        const struct wz* wz,
        const struct wz_image* img,
        uint8_t* out);

struct wz_canvas {
    struct wz_propertycontainer children;
    struct wz_image image;
};

enum wz_property_kind {
    WZ_PROPERTY_KIND_UNSPECIFIED,
    WZ_PROPERTY_KIND_NULL,
    WZ_PROPERTY_KIND_UINT16,
    WZ_PROPERTY_KIND_UINT32,
    WZ_PROPERTY_KIND_FLOAT32,
    WZ_PROPERTY_KIND_FLOAT64,
    WZ_PROPERTY_KIND_STRING,
    WZ_PROPERTY_KIND_CONTAINER,
    WZ_PROPERTY_KIND_NAMEDCONTAINER,
    WZ_PROPERTY_KIND_CANVAS,
    WZ_PROPERTY_KIND_VECTOR,
    WZ_PROPERTY_KIND_SOUND,
    WZ_PROPERTY_KIND_UOL,
};

struct wz_property {
    enum wz_property_kind kind;
    struct wz_encryptedstring name;

    union {
        uint16_t uint16;
        uint32_t uint32;
        float float32;
        double float64;
        struct wz_encryptedstring string;
        struct wz_propertycontainer container;
        struct wz_namedpropertycontainer named_container;
        struct wz_canvas canvas;
        uint32_t vector[2];
        uint8_t* sound;
        struct wz_encryptedstring uol;
    };
};

// wz_property_namedfrom populates the property from offset, using the special
// named property logic. The provided offset is incremented along the way.
struct wz_error wz_property_namedfrom(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off);

// wz_property_from populates the property from offset, incrementing
// it along the way.
struct wz_error wz_property_from(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off);

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
struct wz_error wz_directory_from(
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
struct wz_error wz_directoryentry_from(
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
struct wz_error wz_open(
        struct wz* f,
        const char* filename);

// wz_openfrom opens a wz file from memory at the specified address.
struct wz_error wz_openfrom(
        struct wz* f,
        void* addr);

// wz_close closes a wz file.
struct wz_error wz_close(
        struct wz* f);

#endif
