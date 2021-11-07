#include "wz/directory.hh"
#include "wz/wz.hh"

namespace wz {

Error EntryContainer::Iterator::next(
        Entry* x) {
    if (remaining == 0) return Error();

    if (Error e = Entry::parse(x, &parser)) return e;
    --remaining;

    return Error();
}

Error File::parse(
        File* f,
        Parser* p) {
    // Expect a special header:
    //   Encrypted string (prefix with 0x73) = "Property"
    //   2 bytes ??
    //   PropertyContainer
    uint8_t string_header = 0;
    CHECK(p->u8(&string_header),
            Error::BADREAD) << "failed to read special file header string kind";
    if (string_header != 0x73)
        return error_new(Error::FILEPARSEERROR1)
            << "unknown file name string kind " << string_header;

    String name;
    CHECK(String::parse(&name, p),
            Error::BADREAD) << "failed to read special file header kind";

    // len("Property") == 8
    wchar_t name_value[9] = {0};
    if (name.len >= sizeof(name_value) / sizeof(*name_value))
        return error_new(Error::FILEPARSEERROR2)
            << "unknown file name string (length too long";

    CHECK(name.decrypt(name_value),
            Error::FILEPARSEERROR2) << "failed to decrypt file header kind";
    if (::wcscmp(L"Property", name_value) != 0)
        return error_new(Error::FILEPARSEERROR3)
            << "unknown file name string " << name_value;

    uint16_t unknown = 0;
    CHECK(p->u16(&unknown),
            Error::BADREAD) << "failed to read special file header unknown";

    CHECK(PropertyContainer::parse(
                &f->root,
                p,
                f->base),
            Error::BADREAD) << "failed to read child container";
    return Error();
}

Error Entry::parse(
        Entry* e,
        Parser* p) {
    uint8_t kind = 0;
    CHECK(p->u8(&kind),
            Error::BADREAD) << "failed to read entry kind";

    switch (kind) {
        case 1:
            e->kind = Entry::UNKNOWN;
            break;
        case 2:
        case 4:
            e->kind = Entry::FILE;
            break;
        case 3:
            e->kind = Entry::SUBDIRECTORY;
            break;
        default:
            return error_new(Error::UNKNOWNDIRECTORYENTRYKIND)
                << "unknown directory entry kind " << kind;
    }

    String name;
    if (kind == 1) {
        CHECK(p->u32(&e->unknown.unknown1),
                Error::BADREAD) << "failed to read unknown entry unknown1";
        CHECK(p->u16(&e->unknown.unknown2),
                Error::BADREAD) << "failed to read unknown entry unknown2";
        CHECK(p->offset(&e->unknown.offset),
                Error::BADREAD) << "failed to read unknown entry offset";

        return Error();
    } else if (kind == 2) {
        // Kind 2 files have the names and modified kinds at an offset.
        uint32_t offset = 0;
        CHECK(p->u32(&offset),
                Error::BADREAD) << "failed to read relocated file name offset";

        // At name_at,
        //   u8 kind
        //   String name
        Parser p2 = *p;
        p2.address = p->wz->file.start + p->wz->header.file_start + offset;
        CHECK(p2.u8(&kind),
                Error::BADREAD) << "failed to read relocated file name kind";
        CHECK(String::parse(&name, &p2),
                Error::BADREAD) << "failed to read relocated file name";
    } else {
        // Kinds 3 and 4 have the name inline.
        CHECK(String::parse(&name, p),
                Error::BADREAD) << "failed to read inline file name";
    }

    // We have another check here because the kind may have changed
    // above.
    if (kind == 3) {
        e->kind = Entry::SUBDIRECTORY;
        e->subdirectory.name = name;

        int32_t size = 0;
        CHECK(p->i32_compressed(&size),
                Error::BADREAD) << "failed to read subdirectory size";
        if (size < 0)
            return error_new(Error::BADREAD)
                << "read negative subdirectory size " << size;
        e->subdirectory.size = static_cast<uint32_t>(size);

        int32_t checksum = 0;
        CHECK(p->i32_compressed(&checksum),
                Error::BADREAD) << "failed to read subdirectory checksum";
        e->subdirectory.checksum = static_cast<uint32_t>(checksum);

        uint32_t offset = 0;
        CHECK(p->offset(&offset),
                Error::BADREAD) << "failed to read subdirectory offset";

        Parser p2 = *p;
        p2.address = p->wz->file.start + offset;
        CHECK(Directory::parse(
                    &e->subdirectory.directory,
                    &p2),
                Error::BADREAD) << "failed to read subdirectory";
    } else {
        e->kind = Entry::FILE;
        e->file.name = name;

        int32_t size = 0;
        CHECK(p->i32_compressed(&size),
                Error::BADREAD) << "failed to read file size";
        if (size < 0)
            return error_new(Error::BADREAD)
                << "read negative file size " << size;
        e->file.size = static_cast<uint32_t>(size);

        int32_t checksum = 0;
        CHECK(p->i32_compressed(&checksum),
                Error::BADREAD) << "failed to read file checksum";
        e->file.checksum = static_cast<uint32_t>(checksum);

        uint32_t offset = 0;
        CHECK(p->offset(&offset),
                Error::BADREAD) << "failed to read file offset";

        Parser p2 = *p;
        p2.address = p->wz->file.start + offset;
        e->file.file.base = p2.address;
        CHECK(File::parse(
                    &e->file.file,
                    &p2),
                Error::BADREAD) << "failed to read subdirectory";
    }

    return Error();
}

Error Directory::parse(
        Directory* d,
        Parser* p) {
    CHECK(EntryContainer::parse(
                &d->children,
                p),
            Error::BADREAD) << "failed to read child container";

    return Error();
}

}
