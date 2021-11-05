#include "wz/property.hh"
#include "wz/wz.hh"
#include "wz/directory.hh"

#include <zlib.h>

namespace wz {

extern "C" {
    extern const uint8_t wz_key[];
}

Error String::parse(
        String* s,
        Parser* p) {
    int8_t small_length = 0;
    CHECK(p->i8(&small_length),
            Error::BADREAD) << "failed to read string length";

    if (small_length == 0) {
        s->len = 0;
    } else if (small_length > 0) {
        uint32_t length = 0;
        if (small_length == 0x7F) {
            CHECK(p->u32(&length),
                    Error::BADREAD) << "failed to read two byte string length";
        } else {
            length = static_cast<uint32_t>(small_length);
        }

        s->kind = TWOBYTE;
        s->len = length;
        s->at = p->address;
        p->address += s->len * 2;
    } else {
        uint32_t length = 0;
        if (small_length == -128) {
            CHECK(p->u32(&length),
                    Error::BADREAD) << "failed to read one byte string length";
        } else {
            length = static_cast<uint32_t>(-small_length);
        }

        s->kind = ONEBYTE;
        s->len = length;
        s->at = p->address;
        p->address += s->len;
    }

    if (!p->wz->file.valid(p->address, 1))
        return error_new(Error::INVALIDOFFSET)
            << "string length led past file extents";
    return Error();
}

Error String::parse_withoffset(
        String* s,
        Parser* p,
        const File* f) {
    uint8_t kind = 0;
    CHECK(p->u8(&kind),
            Error::BADREAD) << "failed to read relocated string offset kind";

    switch (kind) {
        case 0x00:
        case 0x73:
            // Inline string.
            {
                CHECK(String::parse(s, p),
                        Error::BADREAD) << "failed to read inline string";
            } break;
        case 0x01:
        case 0x1B:
            // Relative offset.
            {
                int32_t relative = 0;
                CHECK(p->i32(&relative),
                        Error::BADREAD) << "failed to read relative string offset";

                Parser p2 = *p;
                p2.address = f->base + relative;
                CHECK(String::parse(s, &p2),
                        Error::BADREAD) << "failed to read offset string";
            } break;
        default:
            return error_new(Error::UNKNOWNSTRINGOFFSETKIND)
                << "unknown string offset kind " << kind;
    }

    return Error();
}

Error String::decrypt(wchar_t* s) const {
    if (len == 0)
        return Error();

    switch (kind) {
        case ONEBYTE:
            {
                uint8_t mask = 0xAA;
                uint8_t* from = at;
                for (uint32_t i = 0; i < len; ++i) {
                    *s = *from;

                    *s ^= mask;
                    *s ^= wz_key[i & 0xFFFF];
                    ++mask;
                    ++s;
                    ++from;
                }
            } break;
        case TWOBYTE:
            {
                uint16_t mask = 0xAAAA;
                uint16_t* from = reinterpret_cast<uint16_t*>(at);
                for (uint32_t i = 0; i < len; ++i) {
                    *s = *from;

                    *s ^= mask;
                    *s ^= (wz_key[((i * 2) + 1) & 0xFFFF] << 8) + wz_key[(i * 2) & 0xFFFF];
                    ++mask;
                    ++s;
                    ++from;
                }
            } break;
    }

    return Error();
}

Error Image::pixels(uint8_t* out) const {
    uint8_t* compressed_start = data;
    uint32_t compressed_length = length;

    // Some image data requires decrypting.
    if (data[0] != 0x78 || data[1] != 0x9C) {
        compressed_start = out;
        compressed_length = 0;

        uint8_t* end = data + length;

        Parser p;

        uint8_t* to = out;
        uint8_t* from = data;
        while (from < end) {
            uint32_t blocksize = 0;

            p.address = from;
            CHECK(p.u32(&blocksize),
                Error::BADREAD) << "failed to read image blocksize";

            for (uint32_t i = 0; i < blocksize; ++i) {
                *to = *from ^ wz_key[i];
                ++to;
                ++from;
            }
            compressed_length += blocksize;
        }
    }

    // Determine the length of the decompressed data.
    uint32_t decompressed_len = 0;
    switch (format + format2) {
        case 1:
        case 2:
            decompressed_len = 2 * height * width;
            break;
        case 513:
            decompressed_len = 2 * height * width;
            break;
        case 517:
            decompressed_len = height * width / 128;
            break;
        default:
            {
                return error_new(Error::UNKNOWNIMAGEFORMAT)
                    << "unknown image format " << format << " format2 " << format2;
            }
    }

    // For cases where the decompressed data is smaller than the final format,
    // decompress into a later location in the buffer so that we can expand
    // in place.
    uint8_t* decompressed = out + (rawsize() - decompressed_len);

    // Now decompress.
    {
        uint8_t* to = decompressed;

        // Operate in blocks of 4096 bytes.
        enum { blocksize = 4096 };

        uint8_t block[blocksize] = {0};
        z_stream z = {0};
        int z_err = Z_OK;

        z.next_in = compressed_start;
        z.avail_in = compressed_length;
        inflateInit(&z);

        do {
            z.next_out = block;
            z.avail_out = blocksize;

            z_err = inflate(&z, Z_NO_FLUSH);
            if (z_err != Z_STREAM_END && z_err != Z_OK) {
                inflateEnd(&z);

                return error_new(Error::DECOMPRESSIONFAILED)
                    << "zlib decompression failed: " << z_err;
            }

            memcpy(to, block, blocksize - z.avail_out);
            to += blocksize - z.avail_out;
        } while (z_err != Z_STREAM_END && z.avail_in);

        inflateEnd(&z);
    }

    // Finally, convert the pixels (in cases where the format is different).
    switch (format + format2) {
        case 1:
            {
                // Decompressed data is BGRA4, so expand every two bytes into 4.
                uint32_t* to = (uint32_t*) out;
                for (uint32_t i = 0; i < decompressed_len; i += 2) {
                    uint32_t b = decompressed[i] & 0x0F;
                    b |= b << 4;
                    uint32_t g = decompressed[i] & 0xF0;
                    g |= g >> 4;
                    uint32_t r = decompressed[i + 1] & 0x0F;
                    r |= r << 4;
                    uint32_t a = decompressed[i + 1] & 0xF0;
                    a |= a >> 4;

                    *to = (
                            (a << 24) |
                            (b << 16) |
                            (g <<  8) |
                            (r <<  0));
                    ++to;
                }
            } break;
        case 2:
        case 513:
            // Nothing to do here.
            break;
        case 517:
            {
                // Decompressed data is in a wonky format (each byte expands to 4).
                for (uint32_t i = 0; i * 2 < decompressed_len; ++i) {
                    for (uint32_t j = 0; j < 512; ++j) {
                        out[i * 512 + j * 2] = decompressed[i * 2];
                        out[i * 512 + j * 2 + 1] = decompressed[i * 2 + 1];
                    }
                }
            } break;
        default:
            {
                return error_new(Error::UNKNOWNIMAGEFORMAT)
                    << "unknown image format " << format << " format2 " << format2;
            }
    }

    return Error();
}

Error Image::parse(
        Image* x,
        Parser* p) {
    int32_t w = 0;
    CHECK(p->i32_compressed(&w),
            Error::BADREAD) << "failed to read image width";
    if (w < 0)
        return error_new(Error::BADREAD)
            << "read negative image width " << w;
    x->width = static_cast<uint32_t>(w);

    int32_t h = 0;
    CHECK(p->i32_compressed(&h),
            Error::BADREAD) << "failed to read image height";
    if (h < 0)
        return error_new(Error::BADREAD)
            << "read negative image height " << h;
    x->height = static_cast<uint32_t>(h);

    CHECK(p->i32_compressed(&x->format),
            Error::BADREAD) << "failed to read image format";
    CHECK(p->u8(&x->format2),
            Error::BADREAD) << "failed to read image format2";
    CHECK(p->i32(&x->unknown1),
            Error::BADREAD) << "failed to read image unknown1";
    CHECK(p->u32(&x->length),
            Error::BADREAD) << "failed to read image length";
    CHECK(p->u8(&x->unknown2),
            Error::BADREAD) << "failed to read image unknown2";
    x->data = p->address;

    return Error();
}

Error Canvas::parse(
        Canvas* x,
        Parser* p,
        const File* f) {
    uint8_t has_children = 0;
    CHECK(p->u8(&has_children),
            Error::BADREAD) << "failed to read canvas first byte";

    if (has_children) {
        // Skip 2 unknown bytes.
        p->address += 2;

        CHECK(PropertyContainer::parse(&x->children, p, f),
                Error::BADREAD) << "failed to read canvas child container";

        // Unfortunately, we have to parse the entire children container
        // to know where the image starts.
        for (uint32_t i = 0; i < x->children.count; ++i) {
            Property x;
            CHECK(Property::parse(&x, p, f),
                    Error::BADREAD) << "failed to read canvas child " << i;
        }
    } else {
        x->children.count = 0;
    }

    CHECK(Image::parse(&x->image, p),
            Error::BADREAD) << "failed to read canvas image";
    return Error();
}

Error Property::parse(
        Property* x,
        Parser* p,
        const File* f) {
    CHECK(String::parse_withoffset(&x->name, p, f),
            Error::BADREAD) << "failed to read property name";

    uint8_t kind = 0;
    CHECK(p->u8(&kind),
            Error::BADREAD) << "failed to read property kind";

    switch (kind) {
        case 0x00:
            {
                x->kind = VOID;
            } break;
        case 0x02:
        case 0x0B:
            {
                x->kind = UINT16;
                CHECK(p->u16(&x->uint16),
                        Error::BADREAD) << "failed to read u16 property";
            } break;
        case 0x03:
            {
                x->kind = INT32;
                CHECK(p->i32_compressed(&x->int32),
                        Error::BADREAD) << "failed to read i32 property";
            } break;
        case 0x04:
            {
                x->kind = FLOAT32;

                uint8_t kind = 0;
                CHECK(p->u8(&kind),
                        Error::BADREAD) << "failed to read f32 property kind";

                if (kind == 0x80) {
                    CHECK(p->f32(&x->float32),
                            Error::BADREAD) << "failed to read f32 property";
                } else {
                    x->float32 = 0;
                }
            } break;
        case 0x05:
            {
                x->kind = FLOAT64;
                CHECK(p->f64(&x->float64),
                        Error::BADREAD) << "failed to read f64 property";
            } break;
        case 0x08:
            {
                x->kind = STRING;
                CHECK(String::parse_withoffset(&x->string, p, f),
                        Error::BADREAD) << "failed to read string property";
            } break;
        case 0x09:
            {
                uint32_t end_offset = 0;
                CHECK(p->u32(&end_offset),
                        Error::BADREAD) << "failed to read named property end";
                uint8_t* end = p->address + end_offset;

                CHECK(Property::parse_named(x, p, f, &x->name),
                        Error::BADREAD) << "failed to read named property";

                p->address = end;
            } break;
        default:
            {
                return error_new(Error::UNKNOWNPROPERTYKIND)
                    << "unknown property kind " << kind;
            } break;
    }

    return Error();
}

Error Property::parse_named(
        Property* x,
        Parser* p,
        const File* f,
        const String* name) {
    String kind;
    CHECK(String::parse_withoffset(&kind, p, f),
            Error::BADREAD) << "failed to read kind of named property";

    if (name) {
        x->name = *name;
    } else {
        x->name.len = 0;
    }

    // len("Shape2D#Vector2D") == 16
    wchar_t kind_name[17] = {0};
    if (kind.len >= sizeof(kind_name) / sizeof(*kind_name))
        return error_new(Error::UNKNOWNPROPERTYKINDNAME)
            << "unknown named property kind name (len " << kind.len << " too long)";
    CHECK(kind.decrypt(kind_name),
            Error::BADREAD) << "failed to decrypt kind of named property";

    if (::wcscmp(kind_name, L"Property") == 0) {
        x->kind = CONTAINER;

        // Skip unknown 2 bytes;
        p->address += 2;

        CHECK(PropertyContainer::parse(&x->container, p, f),
                Error::BADREAD) << "failed to read property container";
    } else if (::wcscmp(kind_name, L"Canvas") == 0) {
        x->kind = CANVAS;

        // Skip unknown byte.
        ++p->address;

        CHECK(Canvas::parse(&x->canvas, p, f),
                Error::BADREAD) << "failed to read canvas";
    } else if (::wcscmp(kind_name, L"Shape2D#Vector2D") == 0) {
        x->kind = VECTOR;

        CHECK(p->i32_compressed(&x->vector[0]),
                Error::BADREAD) << "failed to read vector x";
        CHECK(p->i32_compressed(&x->vector[1]),
                Error::BADREAD) << "failed to read vector y";
    } else if (::wcscmp(kind_name, L"Shape2D#Convex2D") == 0) {
        x->kind = NAMEDCONTAINER;

        CHECK(NamedPropertyContainer::parse(&x->named_container, p, f),
                Error::BADREAD) << "failed to read named property container";
    } else if (::wcscmp(kind_name, L"Sound_DX8") == 0) {
        x->kind = SOUND;

        x->sound = p->address;
    } else if (::wcscmp(kind_name, L"UOL") == 0) {
        x->kind = UOL;

        // Skip unknown byte.
        ++p->address;

        CHECK(String::parse(&x->uol, p),
                Error::BADREAD) << "failed to read UOL";
    } else
        return error_new(Error::UNKNOWNPROPERTYKINDNAME)
            << "unknown named property kind name " << kind_name;

    return Error();
}

}
