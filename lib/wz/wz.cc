#include "wz/wz.hh"

extern "C" {
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
}

namespace wz {

Error Header::parse(
        Header* h,
        Parser* p) {
    CHECK(p->wstring_fixedlength(&h->ident, 4),
            Error::BADREAD) << "failed to read header ident";
    CHECK(p->u64(&h->file_size),
            Error::BADREAD) << "failed to read file size";
    CHECK(p->u32(&h->file_start),
            Error::BADREAD) << "failed to read file start offset";
    CHECK(p->wstring_nullterminated(&h->copyright),
            Error::BADREAD) << "failed to read copyright string";

    return Error();
}

Wz::~Wz() {
    close();
}

Error Wz::open(
        Wz* wz,
        const char* filename) {
    size_t file_size = 0;

    int ret = _wz_openfileforread(
            &wz->fd,
            &file_size,
            filename);
    if (ret)
        return error_new(Error::OPENFAILED)
            << "failed to open file for read: " << ret;

    ret = _wz_mapfile(
            reinterpret_cast<void**>(&wz->file.start),
            wz->fd,
            file_size);
    if (ret) {
        _wz_closefile(wz->fd);
        return error_new(Error::OPENFAILED)
            << "failed to mmap file: " << ret;
    }

    wz->file.end = wz->file.start + file_size;

    Parser p;
    p.address = wz->file.start;
    p.wz = wz;
    return parse(wz, &p);
}

Error Wz::parse(
        Wz* wz,
        Parser* p) {
    CHECK(Header::parse(&wz->header, p),
            Error::BADREAD) << "failed to read header";

    p->address = wz->file.start + wz->header.file_start;
    CHECK(p->u16(&wz->header.version),
            Error::BADREAD) << "failed to read header version at " << wz->header.file_start;

    // Determine file version.
    for (uint16_t try_version = 1; try_version < 0x7F; ++try_version) {
        uint32_t version_hash = 0;

        std::stringstream ss;
        ss << try_version;
        std::string str = ss.str();
        for (size_t i = 0, l = str.size(); i < l; ++i)
            version_hash = (32 * version_hash) + (int) str[i] + 1;
        uint32_t a = (version_hash >> 24) & 0xFF;
        uint32_t b = (version_hash >> 16) & 0xFF;
        uint32_t c = (version_hash >> 8) & 0xFF;
        uint32_t d = (version_hash >> 0) & 0xFF;
        if ((0xFF ^ a ^ b ^ c ^ d) == wz->header.version) {
            wz->header.version_hash = version_hash;
            wz->version = try_version;
        }
    }

    CHECK(Directory::parse(&wz->root, p),
            Error::BADREAD) << "failed to read root directory";

    return Error();
}

Error Wz::close() {
    if (!fd) return Error();

    int ret = _wz_unmapfile(file.start, file.end - file.start);
    if (ret)
        return error_new(Error::CLOSEFAILED)
            << "failed to unmap file: " << ret;

    ret = _wz_closefile(fd);
    if (ret)
        return error_new(Error::CLOSEFAILED)
            << "failed to close file: " << ret;

    fd = 0;
    return Error();
}

}
