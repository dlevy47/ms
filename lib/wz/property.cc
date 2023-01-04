#include "wz/property.hh"
#include "wz/wz.hh"
#include "wz/directory.hh"

#include <cstring>
#include <fstream>
#define ZLIB_CONST
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

    return Error();
}

Error String::parse_withoffset(
        String* s,
        Parser* p,
        const uint8_t* file_base) {
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
                p2.address = file_base + relative;
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
                const uint8_t* from = at;
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
                const uint16_t* from = reinterpret_cast<const uint16_t*>(at);
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
    // Some image data requires decrypting.
    bool is_encrypted = this->is_encrypted();
    uint32_t encrypted_block_size = 0;
    uint32_t encrypted_block_cursor = 0;

    const uint8_t* source = data;
    const uint8_t* source_end = source + length - 1; // The last byte of image data seems to be, universally, unused.

    uint32_t decompressed_len = rawsize();

    if ((format + format2) == 1) {
        // Type 1 images decompress into a smaller buffer, and then need to
        // be expanded later.
        decompressed_len /= 2;
    } else if ((format + format2) == 517) {
        // Type 517 images are weird: each final row is calculated from a single byte.
        decompressed_len = width * height / 128;
    }

    uint32_t decompressed = 0;

    // Now decompress.
    {
        uint8_t* to = out + (rawsize() - decompressed_len);

        // Operate in blocks of 4096 bytes.
        enum { blocksize = 4096 };

        uint8_t input_block[blocksize] = {0};
        uint8_t output_block[blocksize] = {0};
        z_stream z = {0};
        int z_err = Z_OK;

        inflateInit(&z);

        do {
            z.next_out = output_block;
            z.avail_out = blocksize;

            // Do we need to feed more input data?
            if (z.avail_in == 0) {
                // Are we at the end?
                if (source >= source_end)
                    break;

                if (is_encrypted) {
                    // For encrypted images, we need to decrypt the data and then copy it into
                    // input_block.

                    // Are we in the middle of an input block?
                    if (encrypted_block_cursor >= encrypted_block_size) {
                        // If we aren't, we need to start a new block.

                        // There may not be 4 bytes remaining to read a blocksize. If so, break early.
                        if (source_end - source < 4) {
                            break;
                        }

                        encrypted_block_cursor = 0;
                        encrypted_block_size = *reinterpret_cast<const uint32_t*>(source);
                        source += 4;

                        // Quick check that this encrypted_block_size value is valid.
                        if (source + encrypted_block_size > source_end) {
                            return error_new(Error::BADREAD)
                                << "encrypted image block size extends past image data: " << encrypted_block_size;
                        }
                    }

                    size_t next_chunk_size = encrypted_block_size - encrypted_block_cursor;
                    if (next_chunk_size > blocksize) {
                        next_chunk_size = blocksize;
                    }

                    for (size_t i = 0; i < next_chunk_size; ++i) {
                        input_block[i] = *source ^ wz_key[encrypted_block_cursor];

                        ++encrypted_block_cursor;
                        ++source;
                    }

                    z.next_in = input_block;
                    z.avail_in = next_chunk_size;
                } else {
                    // For images that are not encrypted, just feed zlib directly from source.
                    size_t next_chunk_size = source_end - source;
                    if (next_chunk_size > blocksize) {
                        next_chunk_size = blocksize;
                    }

                    z.next_in = source;
                    z.avail_in = next_chunk_size;
                    source += next_chunk_size;
                }
            }

            z_err = inflate(&z, Z_NO_FLUSH);
            if (z_err < 0) {
                inflateEnd(&z);

                return error_new(Error::DECOMPRESSIONFAILED)
                    << "zlib decompression failed: " << z_err << ": " << (z.msg ? z.msg : "");
            }

            decompressed += blocksize - z.avail_out;
            if (decompressed > decompressed_len) {
                return error_new(Error::DECOMPRESSIONFAILED)
                    << "would decompress past buffer: " << decompressed << " > " << decompressed_len;
            }

            memcpy(to, output_block, blocksize - z.avail_out);
            to += blocksize - z.avail_out;
        } while (z_err != Z_STREAM_END);

        inflateEnd(&z);
    }

    // Perform special expansion.
    if ((format + format2) == 1) {
        uint8_t* from = out + (rawsize() - decompressed_len);
        uint8_t* to = out;

        for (uint32_t i = 0, l = 2 * width * height; i < l; ++i) {
            to[0] = (*from & 0x0F) * 0x11;
            to[1] = ((*from & 0xF0) >> 4) * 0x11;

            to += 2;
            ++from;
        }
    } else if ((format + format2) == 517) {
        uint8_t* from = out + (rawsize() - decompressed_len);
        uint8_t* to = out;

        for (uint32_t i = 0; i < decompressed_len; ++i) {
            for (uint32_t bit = 0; bit < 8; ++bit) {
                uint32_t b = *from & (1 << (7 - bit));
                b >>= 7 - bit;
                b *= 0xFF;

                for (uint32_t k = 0; k < 16; ++k) {
                    to[0] = b;
                    to[1] = b;

                    to += 2;
                }
            }

            ++from;
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
        const uint8_t* file_base) {
    uint8_t has_children = 0;
    CHECK(p->u8(&has_children),
            Error::BADREAD) << "failed to read canvas first byte";

    if (has_children) {
        // Skip 2 unknown bytes.
        p->address += 2;

        CHECK(PropertyContainer::parse(&x->children, p, file_base),
                Error::BADREAD) << "failed to read canvas child container";

        // Unfortunately, we have to parse the entire children container
        // to know where the image starts.
        for (uint32_t i = 0; i < x->children.count; ++i) {
            Property x;
            CHECK(Property::parse(&x, p, file_base),
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
        const uint8_t* file_base) {
    CHECK(String::parse_withoffset(&x->name, p, file_base),
            Error::BADREAD) << "failed to read property name";

    uint8_t kind = 0;
    CHECK(p->u8(&kind),
            Error::BADREAD) << "failed to read property kind";

    switch (kind) {
        case 0x00:
            {
                x->property = Void{};
            } break;
        case 0x02:
        case 0x0B:
            {
                uint16_t u = 0;
                CHECK(p->u16(&u),
                        Error::BADREAD) << "failed to read u16 property";
                x->property = u;
            } break;
        case 0x03:
            {
                int32_t i = 0;
                CHECK(p->i32_compressed(&i),
                        Error::BADREAD) << "failed to read i32 property";
                x->property = i;
            } break;
        case 0x04:
            {
                float f = 0;

                uint8_t kind = 0;
                CHECK(p->u8(&kind),
                        Error::BADREAD) << "failed to read f32 property kind";

                if (kind == 0x80) {
                    CHECK(p->f32(&f),
                            Error::BADREAD) << "failed to read f32 property";
                }
                x->property = f;
            } break;
        case 0x05:
            {
                double d = 0;
                CHECK(p->f64(&d),
                        Error::BADREAD) << "failed to read f64 property";
                x->property = d;
            } break;
        case 0x08:
            {
                String s;
                CHECK(String::parse_withoffset(&s, p, file_base),
                        Error::BADREAD) << "failed to read string property";
                x->property = s;
            } break;
        case 0x09:
            {
                uint32_t end_offset = 0;
                CHECK(p->u32(&end_offset),
                        Error::BADREAD) << "failed to read named property end";
                const uint8_t* end = p->address + end_offset;

                CHECK(Property::parse_named(x, p, file_base, &x->name),
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
        const uint8_t* file_base,
        const String* name) {
    String kind;
    CHECK(String::parse_withoffset(&kind, p, file_base),
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
        PropertyContainer container;

        // Skip unknown 2 bytes;
        p->address += 2;

        CHECK(PropertyContainer::parse(&container, p, file_base),
                Error::BADREAD) << "failed to read property container";
        x->property = std::move(container);
    } else if (::wcscmp(kind_name, L"Canvas") == 0) {
        Canvas canvas;

        // Skip unknown byte.
        ++p->address;

        CHECK(Canvas::parse(&canvas, p, file_base),
                Error::BADREAD) << "failed to read canvas";
        x->property = std::move(canvas);
    } else if (::wcscmp(kind_name, L"Shape2D#Vector2D") == 0) {
        Vector vector;

        CHECK(p->i32_compressed(&vector.x),
                Error::BADREAD) << "failed to read vector x";
        CHECK(p->i32_compressed(&vector.y),
                Error::BADREAD) << "failed to read vector y";
        x->property = vector;
    } else if (::wcscmp(kind_name, L"Shape2D#Convex2D") == 0) {
        NamedPropertyContainer named_container;

        CHECK(NamedPropertyContainer::parse(&named_container, p, file_base),
                Error::BADREAD) << "failed to read named property container";
        x->property = std::move(named_container);
    } else if (::wcscmp(kind_name, L"Sound_DX8") == 0) {
        const uint8_t* sound = p->address;
        x->property = sound;
    } else if (::wcscmp(kind_name, L"UOL") == 0) {
        // Skip unknown byte.
        ++p->address;

        Uol uol;
        CHECK(String::parse_withoffset(&uol.uol, p, file_base),
                Error::BADREAD) << "failed to read UOL";
        x->property = uol;
    } else
        return error_new(Error::UNKNOWNPROPERTYKINDNAME)
            << "unknown named property kind name " << kind_name;

    return Error();
}

}
