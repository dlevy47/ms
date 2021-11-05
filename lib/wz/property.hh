#pragma once

#include <string>

#include "util/error.hh"
#include "wz/parser.hh"

namespace wz {

struct Property;
struct File;

struct String {
    enum Kind {
        ONEBYTE,
        TWOBYTE,
    };

    Kind kind;
    uint8_t* at;
    uint32_t len;

    Error decrypt(wchar_t* s) const;

    static Error parse(
            String* s,
            Parser* p);
    static Error parse_withoffset(
            String* s,
            Parser* p,
            const File* f);
};

struct Image {
    uint32_t width;
    uint32_t height;
    int32_t format;
    uint8_t format2;
    int32_t unknown1;
    uint32_t length;
    uint8_t unknown2;

    uint8_t* data;

    // rawsize returns the uncompressed, unencrypted size of this image's data.
    // Buffers passed to pixels should be at least this big, in bytes.
    uint32_t rawsize() const {
        return width * height * 4;
    }

    // pixels decodes this image's data, and places the pixels into out, in
    // RGBA8 format; that is: 1 byte r, 1 byte g, 1 byte b, 1 byte a.
    Error pixels(uint8_t* out) const;

    static Error parse(
            Image* x,
            Parser* p);
};

template <typename T, Error (*P) (T*, Parser*, const File*)>
struct Container {
    struct Iterator {
        uint32_t remaining;
        Parser parser;
        const File* f;

        Error next(T* x) {
            if (remaining == 0) return Error();

            if (Error e = P(x, &parser, f)) return e;
            --remaining;

            return Error();
        }

        explicit operator bool() const {
            return remaining > 0;
        }
    };

    uint32_t count;
    uint8_t* first;
    const File* f;

    Iterator iterator(const Wz* wz) const {
        Iterator it;
        it.remaining = count;
        it.parser.address = first;
        it.parser.wz = wz;
        it.f = f;
        return it;
    }

    static Error parse(
            Container* c,
            Parser* p,
            const File* f) {
        int32_t count = 0;
        CHECK(p->i32_compressed(&count),
                Error::BADREAD) << "failed to read child count";
        if (count < 0)
            return error_new(Error::BADREAD)
                << "read negative child count" << count;
        c->count = count;
        c->first = p->address;
        c->f = f;

        return Error();
    }
};

inline Error Property_parse(Property* p, Parser* parser, const File* f);
inline Error Property_parse_named(Property* p, Parser* parser, const File* f);

typedef Container<Property, Property_parse> PropertyContainer;
typedef Container<Property, Property_parse_named> NamedPropertyContainer;

struct Canvas {
    Image image;
    Container<Property, Property_parse> children;

    static Error parse(
            Canvas* x,
            Parser* p,
            const File* f);
};

struct Property {
    enum Kind {
        VOID,
        UINT16,
        INT32,
        FLOAT32,
        FLOAT64,
        STRING,
        CONTAINER,
        NAMEDCONTAINER,
        CANVAS,
        VECTOR,
        SOUND,
        UOL,
    };

    Kind kind;
    String name;

    union {
        uint16_t               uint16;
        int32_t                int32;
        float                  float32;
        double                 float64;
        String                 string;
        PropertyContainer      container;
        NamedPropertyContainer named_container;
        Canvas                 canvas;
        int32_t                vector[2];
        uint8_t*               sound;
        String                 uol;
    };

    static Error parse(
            Property* x,
            Parser* p,
            const File* f);
    static Error parse_named(
            Property* x,
            Parser* p,
            const File* f,
            const String* name = nullptr);
};

inline Error Property_parse(Property* p, Parser* parser, const File* f) {
    return Property::parse(p, parser, f);
}

inline Error Property_parse_named(Property* p, Parser* parser, const File* f) {
    return Property::parse_named(p, parser, f);
}

}
