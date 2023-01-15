#pragma once

#include <cstdint>

#include "wz/parser.hh"
#include "wz/property.hh"

namespace wz {

struct Entry;

// EntryContainer provides iteration over entries in a WZ directory.
struct EntryContainer {
    struct Iterator {
        // remaining is the number of entries in this container remaining to
        // be iterated over.
        uint32_t remaining;

        // parser is the parser used to parse entries in this container. This
        // is a different parser object than the one used to read the Wz.
        Parser parser;

        Error next(Entry* x);

        explicit operator bool() const {
            return remaining > 0;
        }
    };

    uint32_t count;
    const uint8_t* first;

    Iterator iterator(const Wz* wz) const {
        Iterator it;
        it.remaining = count;
        it.parser.address = first;
        it.parser.wz = wz;
        return it;
    }

    static Error parse(
            EntryContainer* c,
            Parser* p) {
        int32_t count = 0;
        CHECK(p->i32_compressed(&count),
                Error::BADREAD) << "failed to read child count";
        if (count < 0)
            return error_new(Error::BADREAD)
                << "read negative child count" << count;
        c->count = count;
        c->first = p->address;

        return Error();
    }
};

struct File {
    const uint8_t* base;
    PropertyContainer root;

    static Error parse(
            File* f,
            Parser* p);
};

struct Directory {
    EntryContainer children;

    static Error parse(
            Directory* d,
            Parser* p);
};

struct Entry {
    struct Unknown {
        uint32_t unknown1;
        uint16_t unknown2;
        uint32_t offset;
    };

    struct File {
        String name;
        uint32_t size;
        uint32_t checksum;
        wz::File file;
    };

    struct Directory {
        String name;
        uint32_t size;
        uint32_t checksum;
        wz::Directory directory;
    };

    std::variant<
        Unknown,
        File,
        Directory> entry;

    static Error parse(
            Entry* into,
            Parser* p);
};

}
