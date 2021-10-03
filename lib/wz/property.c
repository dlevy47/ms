#include "wz/property.h"
#include "wz/internal.h"
#include "wz/wz.h"

#include <zlib.h>

struct error wz_encryptedstring_from(
        const struct wz* f,
        struct wz_encryptedstring* s,
        uint8_t** off) {
    uint8_t small_length_u = 0;
    CHECK(_uint8_t_at(f, &small_length_u, off),
        ERROR_KIND_BADREAD, L"failed to read string offset");
    int8_t small_length = (int8_t) small_length_u;

    if (small_length == 0) {
        s->len = 0;
    } else if (small_length > 0) {
        uint32_t length = 0;
        if (small_length == 0x7F) {
            CHECK(_uint32_t_at(f, &length, off),
                ERROR_KIND_BADREAD, L"failed to read two byte string length");
        } else {
            length = (uint32_t) small_length;
        }

        s->kind = WZ_ENCRYPTEDSTRING_KIND_TWOBYTE;
        s->len = length;
        s->at = *off;
        *off += s->len * 2;
    } else {
        uint32_t length = 0;
        if (small_length == -128) {
            CHECK(_uint32_t_at(f, &length, off),
                ERROR_KIND_BADREAD, L"failed to read one byte string length");
        } else {
            length = (uint32_t) -small_length;
        }

        s->kind = WZ_ENCRYPTEDSTRING_KIND_ONEBYTE;
        s->len = length;
        s->at = *off;
        *off += s->len;
    }

    CHECK_OFFSET(f, *off);
    return _NO_ERROR;
}

static struct error wz_encryptedstring_fromwithoffset(
        const struct wz* wz,
        struct wz_encryptedstring* s,
        uint8_t** off,
        uint8_t* base) {
    uint8_t kind = 0;
    CHECK(_uint8_t_at(wz, &kind, off),
        ERROR_KIND_BADREAD, L"failed to read relocated string offset kind");

    switch (kind) {
        case 0x00:
        case 0x73:
            // Inline string.
            {
                CHECK(wz_encryptedstring_from(wz, s, off),
                    ERROR_KIND_BADREAD, L"failed to read inline string");
            } break;
        case 0x01:
        case 0x1B:
            // Relative offset.
            {
                int32_t relative = 0;
                CHECK(_int32_t_at(wz, &relative, off),
                    ERROR_KIND_BADREAD, L"failed to read relative string offset");

                uint8_t* at = base + relative;
                CHECK(wz_encryptedstring_from(wz, s, &at),
                    ERROR_KIND_BADREAD, L"failed to read offset string");
            } break;
        default:
            {
                struct error err = error_new(ERROR_KIND_UNKNOWNSTRINGOFFSETKIND,
                    L"unknown string offset kind");
                error_top(&err)->unknown_string_offset_kind = kind;
                return err;
            } break;
    }

    return _NO_ERROR;
}

struct error wz_encryptedstring_decrypt(
        const struct wz* f,
        const struct wz_encryptedstring* s,
        wchar_t* out) {
    if (s->len == 0) {
        return _NO_ERROR;
    }

    switch (s->kind) {
        case WZ_ENCRYPTEDSTRING_KIND_ONEBYTE:
            {
                CHECK_OFFSET(f, s->at + s->len);

                uint8_t mask = 0xAA;
                uint8_t* from = s->at;
                for (uint32_t i = 0; i < s->len; ++i) {
                    *out = *from;

                    *out ^= mask;
                    *out ^= wz_key[i & 0xFFFF];
                    ++mask;
                    ++out;
                    ++from;
                }
            } break;
        case WZ_ENCRYPTEDSTRING_KIND_TWOBYTE:
            {
                CHECK_OFFSET(f, s->at + (s->len * 2));

                uint16_t mask = 0xAAAA;
                uint16_t* from = (uint16_t*) s->at;
                for (uint32_t i = 0; i < s->len; ++i) {
                    *out = *from;

                    *out ^= mask;
                    *out ^= (wz_key[((i * 2) + 1) & 0xFFFF] << 8) + wz_key[(i * 2) & 0xFFFF];
                    ++mask;
                    ++out;
                    ++from;
                }
            } break;
    }

    return _NO_ERROR;
}

struct error wz_propertycontainer_from(
        const struct wz* wz,
        struct wz_propertycontainer* c,
        uint8_t** off) {
    CHECK(_compressedint_at(wz, &c->count, off),
        ERROR_KIND_BADREAD, L"failed to read property container count");
    c->first = *off;

    return _NO_ERROR;
}

struct error wz_namedpropertycontainer_from(
        const struct wz* wz,
        struct wz_namedpropertycontainer* c,
        uint8_t** off) {
    CHECK(_compressedint_at(wz, &c->count, off),
        ERROR_KIND_BADREAD, L"failed to read named property container count");
    c->first = *off;

    return _NO_ERROR;
}

static struct error wz_image_from(
        const struct wz* wz,
        struct wz_image* img,
        uint8_t** off) {
    CHECK(_compressedint_at(wz, &img->width, off),
        ERROR_KIND_BADREAD, L"failed to read image width");
    CHECK(_compressedint_at(wz, &img->height, off),
        ERROR_KIND_BADREAD, L"failed to read image height");
    CHECK(_compressedint_at(wz, &img->format, off),
        ERROR_KIND_BADREAD, L"failed to read image format");
    CHECK(_uint8_t_at(wz, &img->format2, off),
        ERROR_KIND_BADREAD, L"failed to read image format2");
    CHECK(_uint32_t_at(wz, &img->unknown1, off),
        ERROR_KIND_BADREAD, L"failed to read image unknown1");
    CHECK(_uint32_t_at(wz, &img->length, off),
        ERROR_KIND_BADREAD, L"failed to read image length");
    CHECK(_uint8_t_at(wz, &img->unknown2, off),
        ERROR_KIND_BADREAD, L"failed to read image unknown2");
    img->data = *off;

    return _NO_ERROR;
}

uint32_t wz_image_rawsize(
        const struct wz_image* img) {
    // Each pixel is 4 bytes.
    return img->width * img->height * 4;
}

struct error wz_image_pixels(
        const struct wz* wz,
        const struct wz_image* img,
        uint8_t* out) {
    // Some image data requires decrypting.
    if (img->data[0] != 0x78 || img->data[1] != 0x9C) {
        uint8_t* end = img->data + img->length;

        uint8_t* to = out;
        uint8_t* from = img->data;
        while (from < end) {
            uint32_t blocksize = 0;
            CHECK(_uint32_t_at(wz, &blocksize, &from),
                ERROR_KIND_BADREAD, L"failed to read image blocksize");

            for (uint32_t i = 0; i < blocksize; ++i) {
                *to = *from ^ wz_key[i];
                ++to;
                ++from;
            }
        }
    } else {
        memcpy(out, img->data, img->length);
    }

    // Determine the length of the decompressed data.
    uint32_t decompressed_len = 0;
    switch (img->format + img->format2) {
        case 1:
        case 2:
            decompressed_len = 2 * img->height * img->width;
            break;
        case 513:
            decompressed_len = 2 * img->height * img->width;
            break;
        case 517:
            decompressed_len = img->height * img->width / 128;
            break;
        default:
            {
                struct error err = error_new(ERROR_KIND_UNKNOWNIMAGEFORMAT,
                    L"unknown image format");
                error_top(&err)->unknown_image_format.format = img->format;
                error_top(&err)->unknown_image_format.format2 = img->format2;
                return err;
            }
    }

    // For cases where the decompressed data is smaller than the final format,
    // decompress into a later location in the buffer so that we can expand
    // in place.
    uint8_t* decompressed = out + (wz_image_rawsize(img) - decompressed_len);

    // Now decompress.
    {
        uint8_t* to = decompressed;

        // Operate in blocks of 4096 bytes.
        enum { blocksize = 4096 };

        uint8_t block[blocksize] = {0};
        z_stream z = {0};
        int z_err = Z_OK;

        z.next_in = out;
        z.avail_in = img->length;
        inflateInit(&z);

        do {
            z.next_out = block;
            z.avail_out = blocksize;

            z_err = inflate(&z, Z_NO_FLUSH);
            if (z_err != Z_STREAM_END && z_err != Z_OK) {
                inflateEnd(&z);

                struct error err = error_new(ERROR_KIND_DECOMPRESSIONFAILED,
                    L"zlib decompression failed");
                error_top(&err)->decompression_failed = z_err;
                return err;
            }

            memcpy(to, block, blocksize- z.avail_out);
            to += z.avail_out;
        } while (z_err != Z_STREAM_END && z.avail_in);

        inflateEnd(&z);
    }

    // Finally, convert the pixels (in cases where the format is different).
    switch (img->format + img->format2) {
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
                struct error err = error_new(ERROR_KIND_UNKNOWNIMAGEFORMAT,
                    L"unknown image format");
                error_top(&err)->unknown_image_format.format = img->format;
                error_top(&err)->unknown_image_format.format2 = img->format2;
                return err;
            }
    }

    return _NO_ERROR;
}

struct error wz_canvas_from(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_canvas* c,
        uint8_t** off) {
    uint8_t has_children = 0;
    CHECK(_uint8_t_at(wz, &has_children, off),
        ERROR_KIND_BADREAD, L"failed to read canvas width");

    if (has_children) {
        // Skip 2 unknown bytes.
        *off += 2;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_propertycontainer_from(wz, &c->children, off),
            ERROR_KIND_BADREAD, L"failed to read canvas children");

        // Unfortunately, we have to parse the entire children container
        // to know where the image starts.
        for (uint32_t i = 0; i < c->children.count; ++i) {
            struct wz_property property = {0};
            CHECK(wz_property_from(wz, f, &property, off),
                ERROR_KIND_BADREAD, L"failed to read canvas child %u", i);
        }
    } else {
        c->children.count = 0;
    }

    CHECK(wz_image_from(wz, &c->image, off),
        ERROR_KIND_BADREAD, L"failed to read canvas image");
    return _NO_ERROR;
}

struct error wz_property_namedfrom(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off) {
    struct wz_encryptedstring kind = {0};
    CHECK(wz_encryptedstring_fromwithoffset(wz, &kind, off, f->base),
        ERROR_KIND_BADREAD, L"failed to read kind of named property");

    // len("Shape2D#Vector2D") == 16
    wchar_t kind_name[17] = {0};
    if (kind.len >= sizeof(kind_name) / sizeof(*kind_name)) {
        return error_new(ERROR_KIND_UNKNOWNPROPERTYKINDNAME,
            L"unknown named property kind name");
    }
    CHECK(wz_encryptedstring_decrypt(wz, &kind, kind_name),
        ERROR_KIND_BADREAD, L"failed to decrypt kind of named property");

    if (wcscmp(kind_name, L"Property") == 0) {
        p->kind = WZ_PROPERTY_KIND_CONTAINER;

        // Skip unknown 2 bytes.
        *off += 2;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_propertycontainer_from(wz, &p->container, off),
            ERROR_KIND_BADREAD, L"failed to read property container");
    } else if (wcscmp(kind_name, L"Canvas") == 0) {
        p->kind = WZ_PROPERTY_KIND_CANVAS;

        // Skip unknown byte.
        *off += 1;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_canvas_from(wz, f, &p->canvas, off),
            ERROR_KIND_BADREAD, L"failed to read canvas");
    } else if (wcscmp(kind_name, L"Shape2D#Vector2D") == 0) {
        p->kind = WZ_PROPERTY_KIND_VECTOR;

        CHECK(_compressedint_at(wz, &p->vector[0], off),
            ERROR_KIND_BADREAD, L"failed to read vector x");
        CHECK(_compressedint_at(wz, &p->vector[1], off),
            ERROR_KIND_BADREAD, L"failed to read vector y");
    } else if (wcscmp(kind_name, L"Shape2D#Convex2D") == 0) {
        p->kind = WZ_PROPERTY_KIND_NAMEDCONTAINER;

        CHECK(wz_namedpropertycontainer_from(wz, &p->named_container, off),
            ERROR_KIND_BADREAD, L"failed to read named property container");
    } else if (wcscmp(kind_name, L"Sound_DX8") == 0) {
        p->kind = WZ_PROPERTY_KIND_SOUND;

        p->sound = *off;
    } else if (wcscmp(kind_name, L"UOL") == 0) {
        p->kind = WZ_PROPERTY_KIND_UOL;

        // Skip unknown byte.
        *off += 1;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_encryptedstring_fromwithoffset(wz, &p->uol, off, f->base),
            ERROR_KIND_BADREAD, L"failed to read UOL");
    } else {
        return error_new(ERROR_KIND_UNKNOWNPROPERTYKINDNAME,
            L"unknown named property kind name");
    }

    return _NO_ERROR;
}

struct error wz_property_from(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off) {
    CHECK(wz_encryptedstring_fromwithoffset(wz, &p->name, off, f->base),
        ERROR_KIND_BADREAD, L"failed to read property name");

    uint8_t kind = 0;
    CHECK(_uint8_t_at(wz, &kind, off),
        ERROR_KIND_BADREAD, L"failed to read property kind");

    switch (kind) {
        case 0x00:
            {
                p->kind = WZ_PROPERTY_KIND_NULL;
            } break;
        case 0x02:
        case 0x0B:
            {
                p->kind = WZ_PROPERTY_KIND_UINT16;
                CHECK(_uint16_t_at(wz, &p->uint16, off),
                    ERROR_KIND_BADREAD, L"failed to read u16 property");
            } break;
        case 0x03:
            {
                p->kind = WZ_PROPERTY_KIND_UINT32;
                CHECK(_compressedint_at(wz, &p->uint32, off),
                    ERROR_KIND_BADREAD, L"failed to read u32 property");
            } break;
        case 0x04:
            {
                p->kind = WZ_PROPERTY_KIND_FLOAT32;

                uint8_t kind = 0;
                CHECK(_uint8_t_at(wz, &kind, off),
                    ERROR_KIND_BADREAD, L"failed to read f32 property kind");

                if (kind == 0x80) {
                    CHECK(_float_at(wz, &p->float32, off),
                        ERROR_KIND_BADREAD, L"failed to read f32 property");
                } else {
                    p->float32 = 0;
                }
            } break;
        case 0x05:
            {
                p->kind = WZ_PROPERTY_KIND_FLOAT64;
                CHECK(_double_at(wz, &p->float64, off),
                    ERROR_KIND_BADREAD, L"failed to read f64 property");
            } break;
        case 0x08:
            {
                p->kind = WZ_PROPERTY_KIND_STRING;
                CHECK(wz_encryptedstring_fromwithoffset(wz, &p->string, off, f->base),
                    ERROR_KIND_BADREAD, L"failed to read string property");
            } break;
        case 0x09:
            {
                uint32_t end_offset = 0;
                CHECK(_uint32_t_at(wz, &end_offset, off),
                    ERROR_KIND_BADREAD, L"failed to read named property end");
                uint8_t* end = *off + end_offset;

                CHECK(wz_property_namedfrom(wz, f, p, off),
                    ERROR_KIND_BADREAD, L"failed to read named property");

                *off = end;
                CHECK_OFFSET(wz, *off);
            } break;
        default:
            {
                struct error err = error_new(ERROR_KIND_UNKNOWNPROPERTYKIND,
                    L"unknown property kind");
                error_top(&err)->unknown_property_kind = kind;
            } break;
    }

    return _NO_ERROR;
}