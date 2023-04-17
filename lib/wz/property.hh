#pragma once

#include <string>
#include <variant>

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
    const uint8_t* at;
    uint32_t len;

    Error decrypt(wchar_t* s) const;

    static Error parse(
        String* s,
        Parser* p);
    static Error parse_withoffset(
        String* s,
        Parser* p,
        const uint8_t* file_base);
};

struct Image {
    uint32_t width;
    uint32_t height;
    int32_t format;
    uint8_t format2;
    int32_t unknown1;
    uint32_t length;
    uint8_t unknown2;

    const uint8_t* data;

    // rawsize returns the uncompressed, unencrypted size of this image's data.
    // Buffers passed to pixels should be at least this big, in bytes.
    uint32_t rawsize() const {
        switch (format + format2) {
        case 1:
            // BGRA8.
            return width * height * 4;
        case 2:
            // BGRA8.
            return width * height * 4;
        case 513:
            // BGR 5_6_5_REV (or, RGB 5_6_5)
            return width * height * 2;
        case 517:
            // BGR 5_6_5_REV?
            return width * height * 2;
        }

        // TODO: handle errors here?
        return 0;
    }

    // pixels decodes this image's data, and places the pixels into out. The
    // format of the image data depends on the value of format + format2.
    Error pixels(uint8_t* out) const;

    // is_encrypted returns whether this image's data is encrypted, based on
    // a guess about valid zlib headers.
    bool is_encrypted() const {
        return length > 2 && !(
            (data[0] == 0x78 && data[1] == 0x9C) ||
            (data[0] == 0x78 && data[1] == 0xDA) ||
            (data[0] == 0x78 && data[1] == 0x01) ||
            (data[0] == 0x78 && data[1] == 0x5E));
    }

    static Error parse(
        Image* x,
        Parser* p);
};

template <typename T, Error(*P) (T*, Parser*, const uint8_t*)>
struct Container {
    struct Iterator {
        uint32_t remaining;
        Parser parser;
        const uint8_t* file_base;

        Error next(T* x) {
            if (remaining == 0) return Error();

            if (Error e = P(x, &parser, file_base)) return e;
            --remaining;

            return Error();
        }

        explicit operator bool() const {
            return remaining > 0;
        }
    };

    uint32_t count;
    const uint8_t* first;
    const uint8_t* file_base;

    Iterator iterator(const Wz* wz) const {
        Iterator it;
        it.remaining = count;
        it.parser.address = first;
        it.parser.wz = wz;
        it.file_base = file_base;
        return it;
    }

    static Error parse(
        Container* c,
        Parser* p,
        const uint8_t* file_base) {
        int32_t count = 0;
        CHECK(p->i32_compressed(&count),
            Error::BADREAD) << "failed to read child count";
        if (count < 0)
            return error_new(Error::BADREAD)
            << "read negative child count" << count;
        c->count = count;
        c->first = p->address;
        c->file_base = file_base;

        return Error();
    }
};

inline Error Property_parse(Property* p, Parser* parser, const uint8_t* file_base);
inline Error Property_parse_named(Property* p, Parser* parser, const uint8_t* file_base);

typedef Container<Property, Property_parse> PropertyContainer;
typedef Container<Property, Property_parse_named> NamedPropertyContainer;

struct Canvas {
    Image image;
    Container<Property, Property_parse> children;

    static Error parse(
        Canvas* x,
        Parser* p,
        const uint8_t* file_base);
};

struct Vector {
    int32_t x;
    int32_t y;
};

struct Uol {
    String uol;
};

struct Void {
};

struct Property {
    String name;

    std::variant<
        Void,
        uint16_t,
        int32_t,
        float,
        double,
        String,
        PropertyContainer,
        NamedPropertyContainer,
        Canvas,
        Vector,
        const uint8_t*,
        Uol> property;

    static Error parse(
        Property* x,
        Parser* p,
        const uint8_t* file_base);
    static Error parse_named(
        Property* x,
        Parser* p,
        const uint8_t* file_base,
        const String* name = nullptr);
};

inline Error Property_parse(Property* p, Parser* parser, const uint8_t* file_base) {
    return Property::parse(p, parser, file_base);
}

inline Error Property_parse_named(Property* p, Parser* parser, const uint8_t* file_base) {
    return Property::parse_named(p, parser, file_base);
}

}
