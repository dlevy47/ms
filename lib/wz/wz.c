#include "wz/wz.h"
#include "wz/internal.h"

#include <stdio.h>
#include <stdlib.h>

static struct error wz_file_from(
        const struct wz* wz,
        struct wz_file* f,
        uint8_t** off) {
    // Expect a special header:
    // Encrypted string (prefix with 0x73) = "Property"
    // 2 bytes ??
    // struct wz_propertycontainer
    uint8_t string_header = 0;
    CHECK(_uint8_t_at(wz, &string_header, off),
        ERROR_KIND_BADREAD, L"failed to read special file header string kind");
    if (string_header != 0x73) {
        return error_new(ERROR_KIND_FILEPARSEERROR1,
            L"unknown file name string kind");
    }

    struct wz_encryptedstring name = {0};
    CHECK(wz_encryptedstring_from(wz, &name, off),
        ERROR_KIND_BADREAD, L"failed to read special file header kind");

    // len("Property") == 8
    wchar_t name_value[9] = {0};
    if (name.len >= sizeof(name_value) / sizeof(*name_value)) {
        return error_new(ERROR_KIND_FILEPARSEERROR2,
            L"unknown file name string");
    }

    CHECK(wz_encryptedstring_decrypt(wz, &name, name_value),
        ERROR_KIND_BADREAD, L"failed to read decrypt file header kind");
    if (wcscmp(L"Property", name_value) != 0) {
        return error_new(ERROR_KIND_FILEPARSEERROR3,
            L"unknown file name string");
    }

    uint16_t unknown = 0;
    CHECK(_uint16_t_at(wz, &unknown, off),
        ERROR_KIND_BADREAD, L"failed to read special file header unknown u16");

    CHECK(wz_propertycontainer_from(wz, &f->root, off),
        ERROR_KIND_BADREAD, L"failed to read file root");
    return _NO_ERROR;
}

struct error wz_directoryentry_from(
        const struct wz* f,
        struct wz_directoryentry* d,
        uint8_t** off) {
    uint8_t kind = 0;
    CHECK(_uint8_t_at(f, &kind, off),
        ERROR_KIND_BADREAD, L"failed to read directory entry kind");

    switch (kind) {
        case 1:
            d->kind = WZ_DIRECTORYENTRY_KIND_UNKNOWN;
            break;
        case 2:
        case 4:
            d->kind = WZ_DIRECTORYENTRY_KIND_FILE;
            break;
        case 3:
            d->kind = WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY;
            break;
        default:
            {
                struct error err = error_new(ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND,
                    L"unknown directory entry kind");
                error_top(&err)->unknown_directoryentry_kind = kind;
                return err;
            }
    }

    struct wz_encryptedstring name = {0};
    if (kind == 1) {
        d->kind = WZ_DIRECTORYENTRY_KIND_UNKNOWN;
        CHECK(_uint32_t_at(f, &d->unknown.unknown1, off),
            ERROR_KIND_BADREAD, L"failed to read unknown directory entry unknown1");
        CHECK(_uint16_t_at(f, &d->unknown.unknown2, off),
            ERROR_KIND_BADREAD, L"failed to read unknown directory entry unknown2");
        CHECK(_offset_at(f, &d->unknown.offset, off),
            ERROR_KIND_BADREAD, L"failed to read unknown directory entry offset");
        return _NO_ERROR;
    } else if (kind == 2) {
        // Kind 2 files have the names and modified kinds at an offset.
        uint32_t offset = 0;
        CHECK(_uint32_t_at(f, &offset, off),
            ERROR_KIND_BADREAD, L"failed to read relocated name file name offset");

        // At name_at,
        //   u8 kind
        //   encryptedstring name
        uint8_t* name_at = f->file_addr + f->header.file_start + offset;
        CHECK_OFFSET(f, name_at);
        CHECK(wz_encryptedstring_from(f, &name, &name_at),
            ERROR_KIND_BADREAD, L"failed to read relocated name file name");
        CHECK(_uint8_t_at(f, &kind, &name_at),
            ERROR_KIND_BADREAD, L"failed to read relocated name file name kind");
    } else {
        // Kinds 3 and 4 have the name inline.
        wz_encryptedstring_from(f, &name, off);
    }

    // We have another check here because the kind may have changed
    // above.
    if (kind == 3) {
        d->kind = WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY;

        d->subdirectory.name = name;

        int32_t size = 0;
        CHECK(_compressedint_at(f, &size, off),
            ERROR_KIND_BADREAD, L"failed to read subdirectory size");
        if (size < 0)
            return error_new(
                    ERROR_KIND_BADREAD, L"read negative subdirectory size %d", size);
        d->subdirectory.size = (uint32_t) size;

        int32_t checksum = 0;
        CHECK(_compressedint_at(f, &checksum, off),
            ERROR_KIND_BADREAD, L"failed to read subdirectory checksum");
        if (size < 0)
            return error_new(
                    ERROR_KIND_BADREAD, L"read negative subdirectory checksum %d", checksum);
        d->subdirectory.checksum = (uint32_t) checksum;

        uint32_t offset = 0;
        CHECK(_offset_at(f, &offset, off),
            ERROR_KIND_BADREAD, L"failed to read subdirectory offset");

        uint8_t* subdirectory_at = f->file_addr + offset;
        CHECK(wz_directory_from(f, &d->subdirectory.subdirectory, &subdirectory_at),
            ERROR_KIND_BADREAD, L"failed to read subdirectory");
    } else {
        d->kind = WZ_DIRECTORYENTRY_KIND_FILE;

        d->file.name = name;

        int32_t size = 0;
        CHECK(_compressedint_at(f, &size, off),
            ERROR_KIND_BADREAD, L"failed to read file size");
        if (size < 0)
            return error_new(
                    ERROR_KIND_BADREAD, L"read negative file size %d", size);
        d->file.size = (uint32_t) size;

        int32_t checksum = 0;
        CHECK(_compressedint_at(f, &checksum, off),
            ERROR_KIND_BADREAD, L"failed to read file checksum");
        if (size < 0)
            return error_new(
                    ERROR_KIND_BADREAD, L"read negative file checksum %d", checksum);
        d->file.checksum = (uint32_t) checksum;

        uint32_t offset = 0;
        CHECK(_offset_at(f, &offset, off),
            ERROR_KIND_BADREAD, L"failed to read file offset");

        uint8_t* file_at = f->file_addr + offset;
        CHECK_OFFSET(f, file_at);

        d->file.file.base = file_at;
        CHECK(wz_file_from(f, &d->file.file, &file_at),
            ERROR_KIND_BADREAD, L"failed to read file");
    }

    return _NO_ERROR;
}

struct error wz_directory_from(
        const struct wz* f,
        struct wz_directory* d,
        uint8_t** off) {
    int32_t count = 0;
    CHECK(_compressedint_at(f, &count, off),
        ERROR_KIND_BADREAD, L"failed to read child count");
    if (count < 0)
        return error_new(
                ERROR_KIND_BADREAD, L"read negative child count %d", count);
    d->count = (uint32_t) count;
    d->first_entry = *off;

    return _NO_ERROR;
}

// wz_header_from populates the values in the provided header object with
// values from a file mapped into memory at off, incrementing off along the way.
static struct error wz_header_from(
        struct wz* f,
        struct wz_header* header,
        uint8_t** off) {
    CHECK(_lenstr_at(f, header->ident, off, 4),
        ERROR_KIND_BADREAD, L"failed to read header ident");
    CHECK(_uint64_t_at(f, &header->file_size, off),
        ERROR_KIND_BADREAD, L"failed to read file size");
    CHECK(_uint32_t_at(f, &header->file_start, off),
        ERROR_KIND_BADREAD, L"failed to read file start offset");
    CHECK(_nullstr_at(f, &header->copyright, off),
        ERROR_KIND_BADREAD, L"failed to read copyright string");

    return _NO_ERROR;
}

struct error wz_open(
        struct wz* f,
        const char* filename) {
    int ret = 0;

    ret = _wz_openfileforread(
            &f->fd,
            &f->file_size,
            filename);
    if (ret) {
        struct error err = error_new(ERROR_KIND_ERRNO,
            L"failed to open file for read: \"%s\"", filename);
        error_top(&err)->errno_ = ret;
        return err;
    }

    void* addr;
    ret = _wz_mapfile(
            &addr,
            f->fd,
            f->file_size);
    if (ret) {
        _wz_closefile(f->fd);

        struct error err = error_new(ERROR_KIND_ERRNO,
            L"failed to mmap file: \"%s\"", filename);
        error_top(&err)->errno_ = ret;
        return err;
    }

    return wz_openfrom(f, addr);
}

struct error wz_openfrom(
        struct wz* f,
        void* addr_v) {
    f->file_addr = (uint8_t*) addr_v;

    uint8_t* header_addr = f->file_addr;
    CHECK(wz_header_from(f, &f->header, &header_addr),
        ERROR_KIND_BADREAD, L"failed to read header");

    uint8_t* root_addr = f->file_addr + f->header.file_start;
    CHECK(_uint16_t_at(f, &f->header.version, &root_addr),
        ERROR_KIND_BADREAD, L"failed to read header version");

    // Determine file version.
    for (uint16_t try_version = 1; try_version < 0x7F; ++try_version) {
        uint32_t version_hash = 0;

        char str[5] = {0};
        int l = sprintf(str, "%d", try_version);
        for (int i = 0; i < l; ++i)
            version_hash = (32 * version_hash) + (int) str[i] + 1;
        uint32_t a = (version_hash >> 24) & 0xFF;
        uint32_t b = (version_hash >> 16) & 0xFF;
        uint32_t c = (version_hash >> 8) & 0xFF;
        uint32_t d = (version_hash >> 0) & 0xFF;
        if ((0xFF ^ a ^ b ^ c ^ d) == f->header.version) {
            f->header.version_hash = version_hash;
            f->version = try_version;
        }
    }

    CHECK(wz_directory_from(f, &f->root, &root_addr),
        ERROR_KIND_BADREAD, L"failed to read root");

    return _NO_ERROR;
}

struct error wz_close(
        struct wz* f) {
    struct error err = {0};

    if (f->fd) {
        int ret = 0;

        ret = _wz_unmapfile(f->file_addr, f->file_size);
        if (ret) {
            err = error_new(ERROR_KIND_ERRNO,
                L"failed to unmap file");
            error_top(&err)->errno_ = errno;
        }

        ret = _wz_closefile(f->fd);
        if (ret) {
            err = error_new(ERROR_KIND_ERRNO,
                L"failed to close file");
            error_top(&err)->errno_ = errno;
        }
    }

    return err;
}
