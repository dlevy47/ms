#include "wz/wz.h"
#include "wz/wz-internal.h"

#include <zlib.h>

const struct wz_error _NO_ERROR = {0};

static struct wz_error wz_encryptedstring_from(
        const struct wz* f,
        struct wz_encryptedstring* s,
        uint8_t** off) {
    uint8_t small_length_u = 0;
    CHECK(_uint8_t_at(f, &small_length_u, off));
    int8_t small_length = (int8_t) small_length_u;

    if (small_length == 0) {
        s->len = 0;
    } else if (small_length > 0) {
        uint32_t length = 0;
        if (small_length == 0x7F) {
            CHECK(_uint32_t_at(f, &length, off));
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
            CHECK(_uint32_t_at(f, &length, off));
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

static struct wz_error wz_encryptedstring_fromwithoffset(
        const struct wz* wz,
        struct wz_encryptedstring* s,
        uint8_t** off,
        uint8_t* base) {
    uint8_t kind = 0;
    CHECK(_uint8_t_at(wz, &kind, off));

    switch (kind) {
        case 0x00:
        case 0x73:
            // Inline string.
            {
                CHECK(wz_encryptedstring_from(wz, s, off));
            } break;
        case 0x01:
        case 0x1B:
            // Relative offset.
            {
                int32_t relative = 0;
                CHECK(_int32_t_at(wz, &relative, off));

                uint8_t* at = base + relative;
                CHECK(wz_encryptedstring_from(wz, s, &at));
            } break;
        default:
            {
                struct wz_error err = {
                    .kind = WZ_ERROR_KIND_UNKNOWNSTRINGOFFSETKIND,
                    .unknown_string_offset_kind = kind,
                };
                return err;
            } break;
    }

    return _NO_ERROR;
}

struct wz_error wz_encryptedstring_decrypt(
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

struct wz_error wz_propertycontainer_from(
        const struct wz* wz,
        struct wz_propertycontainer* c,
        uint8_t** off) {
    CHECK(_compressedint_at(wz, &c->count, off));
    c->first = *off;

    return _NO_ERROR;
}

struct wz_error wz_namedpropertycontainer_from(
        const struct wz* wz,
        struct wz_namedpropertycontainer* c,
        uint8_t** off) {
    CHECK(_compressedint_at(wz, &c->count, off));
    c->first = *off;

    return _NO_ERROR;
}

static struct wz_error wz_image_from(
        const struct wz* wz,
        struct wz_image* img,
        uint8_t** off) {
    CHECK(_compressedint_at(wz, &img->width, off));
    CHECK(_compressedint_at(wz, &img->height, off));
    CHECK(_compressedint_at(wz, &img->format, off));
    CHECK(_uint8_t_at(wz, &img->format2, off));
    CHECK(_uint32_t_at(wz, &img->unknown1, off));
    CHECK(_uint32_t_at(wz, &img->length, off));
    CHECK(_uint8_t_at(wz, &img->unknown2, off));
    img->data = *off;

    return _NO_ERROR;
}

uint32_t wz_image_rawsize(
        const struct wz_image* img) {
    // Each pixel is 4 bytes.
    return img->width * img->height * 4;
}

struct wz_error wz_image_pixels(
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
            CHECK(_uint32_t_at(wz, &blocksize, &from));

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
                struct wz_error err = {
                    .kind = WZ_ERROR_KIND_UNKNOWNIMAGEFORMAT,
                    .unknown_image_format = {
                        .format = img->format,
                        .format2 = img->format2,
                    },
                };
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
        const uint32_t blocksize = 4096;

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

                struct wz_error err = {
                    .kind = WZ_ERROR_KIND_DECOMPRESSIONFAILED,
                    .decompression_failed = z_err,
                };
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
                struct wz_error err = {
                    .kind = WZ_ERROR_KIND_UNKNOWNIMAGEFORMAT,
                    .unknown_image_format = {
                        .format = img->format,
                        .format2 = img->format2,
                    },
                };
                return err;
            }
    }

    return _NO_ERROR;
}

struct wz_error wz_canvas_from(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_canvas* c,
        uint8_t** off) {
    uint8_t has_children = 0;
    CHECK(_uint8_t_at(wz, &has_children, off));

    if (has_children) {
        // Skip 2 unknown bytes.
        *off += 2;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_propertycontainer_from(wz, &c->children, off));

        // Unfortunately, we have to parse the entire children container
        // to know where the image starts.
        for (uint32_t i = 0; i < c->children.count; ++i) {
            struct wz_property property = {0};
            CHECK(wz_property_from(wz, f, &property, off));
        }
    } else {
        c->children.count = 0;
    }

    CHECK(wz_image_from(wz, &c->image, off));
    return _NO_ERROR;
}

struct wz_error wz_property_namedfrom(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off) {
    struct wz_encryptedstring kind = {0};
    CHECK(wz_encryptedstring_fromwithoffset(wz, &kind, off, f->base));

    // len("Shape2D#Vector2D") == 16
    wchar_t kind_name[17] = {0};
    if (kind.len >= sizeof(kind_name) / sizeof(*kind_name)) {
        struct wz_error err = {
            .kind = WZ_ERROR_KIND_UNKNOWNPROPERTYKINDNAME,
        };
        return err;
    }
    CHECK(wz_encryptedstring_decrypt(wz, &kind, kind_name));

    if (wcscmp(kind_name, L"Property") == 0) {
        p->kind = WZ_PROPERTY_KIND_CONTAINER;

        // Skip unknown 2 bytes.
        *off += 2;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_propertycontainer_from(wz, &p->container, off));
    } else if (wcscmp(kind_name, L"Canvas") == 0) {
        p->kind = WZ_PROPERTY_KIND_CANVAS;

        // Skip unknown byte.
        *off += 1;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_canvas_from(wz, f, &p->canvas, off));
    } else if (wcscmp(kind_name, L"Shape2D#Vector2D") == 0) {
        p->kind = WZ_PROPERTY_KIND_VECTOR;

        CHECK(_compressedint_at(wz, &p->vector[0], off));
        CHECK(_compressedint_at(wz, &p->vector[1], off));
    } else if (wcscmp(kind_name, L"Shape2D#Convex2D") == 0) {
        p->kind = WZ_PROPERTY_KIND_NAMEDCONTAINER;

        CHECK(wz_namedpropertycontainer_from(wz, &p->named_container, off));
    } else if (wcscmp(kind_name, L"Sound_DX8") == 0) {
        p->kind = WZ_PROPERTY_KIND_SOUND;

        p->sound = *off;
    } else if (wcscmp(kind_name, L"UOL") == 0) {
        p->kind = WZ_PROPERTY_KIND_UOL;

        // Skip unknown byte.
        *off += 1;
        CHECK_OFFSET(wz, *off);

        CHECK(wz_encryptedstring_fromwithoffset(wz, &p->uol, off, f->base));
    } else {
        struct wz_error err = {
            .kind = WZ_ERROR_KIND_UNKNOWNPROPERTYKINDNAME,
        };
        return err;
    }

    return _NO_ERROR;
}

struct wz_error wz_property_from(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off) {
    CHECK(wz_encryptedstring_fromwithoffset(wz, &p->name, off, f->base));

    uint8_t kind = 0;
    CHECK(_uint8_t_at(wz, &kind, off));

    switch (kind) {
        case 0x00:
            {
                p->kind = WZ_PROPERTY_KIND_NULL;
            } break;
        case 0x02:
        case 0x0B:
            {
                p->kind = WZ_PROPERTY_KIND_UINT16;
                CHECK(_uint16_t_at(wz, &p->uint16, off));
            } break;
        case 0x03:
            {
                p->kind = WZ_PROPERTY_KIND_UINT32;
                CHECK(_compressedint_at(wz, &p->uint32, off));
            } break;
        case 0x04:
            {
                p->kind = WZ_PROPERTY_KIND_FLOAT32;

                uint8_t kind = 0;
                CHECK(_uint8_t_at(wz, &kind, off));

                if (kind == 0x80) {
                    CHECK(_float_at(wz, &p->float32, off));
                } else {
                    p->float32 = 0;
                }
            } break;
        case 0x05:
            {
                p->kind = WZ_PROPERTY_KIND_FLOAT64;
                CHECK(_double_at(wz, &p->float64, off));
            } break;
        case 0x08:
            {
                p->kind = WZ_PROPERTY_KIND_STRING;
                CHECK(wz_encryptedstring_fromwithoffset(wz, &p->string, off, f->base));
            } break;
        case 0x09:
            {
                uint32_t end_offset = 0;
                CHECK(_uint32_t_at(wz, &end_offset, off));
                uint8_t* end = *off + end_offset;

                CHECK(wz_property_namedfrom(wz, f, p, off));

                *off = end;
                CHECK_OFFSET(wz, *off);
            } break;
        default:
            {
                struct wz_error err = {
                    .kind = WZ_ERROR_KIND_UNKNOWNPROPERTYKIND,
                    .unknown_property_kind = kind,
                };
                return err;
            } break;
    }

    return _NO_ERROR;
}

static struct wz_error wz_file_from(
        const struct wz* wz,
        struct wz_file* f,
        uint8_t** off) {
    // Expect a special header:
    // Encrypted string (prefix with 0x73) = "Property"
    // 2 bytes ??
    // struct wz_propertycontainer
    uint8_t string_header = 0;
    CHECK(_uint8_t_at(wz, &string_header, off));
    if (string_header != 0x73) {
        struct wz_error err = {
            .kind = WZ_ERROR_KIND_FILEPARSEERROR1,
        };
        return err;
    }

    struct wz_encryptedstring name = {0};
    CHECK(wz_encryptedstring_from(wz, &name, off));

    // len("Property") == 8
    wchar_t name_value[9] = {0};
    if (name.len >= sizeof(name_value) / sizeof(*name_value)) {
        struct wz_error err = {
            .kind = WZ_ERROR_KIND_FILEPARSEERROR2,
        };
        return err;
    }

    CHECK(wz_encryptedstring_decrypt(wz, &name, name_value));
    if (wcscmp(L"Property", name_value) != 0) {
        struct wz_error err = {
            .kind = WZ_ERROR_KIND_FILEPARSEERROR2,
        };
        return err;
    }

    uint16_t unknown = 0;
    CHECK(_uint16_t_at(wz, &unknown, off));

    CHECK(wz_propertycontainer_from(wz, &f->root, off));
    return _NO_ERROR;
}

struct wz_error wz_directoryentry_from(
        const struct wz* f,
        struct wz_directoryentry* d,
        uint8_t** off) {
    uint8_t kind = 0;
    CHECK(_uint8_t_at(f, &kind, off));

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
                struct wz_error err = {
                    .kind = WZ_ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND,
                    .unknown_directoryentry_kind = kind,
                };
                return err;
            }
    }

    struct wz_encryptedstring name = {0};
    if (kind == 1) {
        d->kind = WZ_DIRECTORYENTRY_KIND_UNKNOWN;
        CHECK(_uint32_t_at(f, &d->unknown.unknown1, off));
        CHECK(_uint16_t_at(f, &d->unknown.unknown2, off));
        CHECK(_offset_at(f, &d->unknown.offset, off));
        return _NO_ERROR;
    } else if (kind == 2) {
        // Kind 2 files have the names and modified kinds at an offset.
        uint32_t offset = 0;
        CHECK(_uint32_t_at(f, &offset, off));

        // At name_at,
        //   u8 kind
        //   encryptedstring name
        uint8_t* name_at = f->file_addr + f->header.file_start + offset;
        CHECK_OFFSET(f, name_at);
        wz_encryptedstring_from(f, &name, &name_at);
        CHECK(_uint8_t_at(f, &kind, &name_at));
    } else {
        // Kinds 3 and 4 have the name inline.
        wz_encryptedstring_from(f, &name, off);
    }

    // We have another check here because the kind may have changed
    // above.
    if (kind == 3) {
        d->kind = WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY;

        d->subdirectory.name = name;
        CHECK(_compressedint_at(f, &d->subdirectory.size, off));
        CHECK(_compressedint_at(f, &d->subdirectory.checksum, off));

        uint32_t offset = 0;
        CHECK(_offset_at(f, &offset, off));

        uint8_t* subdirectory_at = f->file_addr + offset;
        CHECK(wz_directory_from(f, &d->subdirectory.subdirectory, &subdirectory_at));
    } else {
        d->kind = WZ_DIRECTORYENTRY_KIND_FILE;

        d->file.name = name;
        CHECK(_compressedint_at(f, &d->file.size, off));
        CHECK(_compressedint_at(f, &d->file.checksum, off));

        uint32_t offset = 0;
        CHECK(_offset_at(f, &offset, off));

        uint8_t* file_at = f->file_addr + offset;
        CHECK_OFFSET(f, file_at);

        d->file.file.base = file_at;
        CHECK(wz_file_from(f, &d->file.file, &file_at));
    }

    return _NO_ERROR;
}

struct wz_error wz_directory_from(
        const struct wz* f,
        struct wz_directory* d,
        uint8_t** off) {
    CHECK(_compressedint_at(f, &d->count, off));
    d->first_entry = *off;

    return _NO_ERROR;
}

// wz_header_from populates the values in the provided header object with
// values from a file mapped into memory at off, incrementing off along the way.
static struct wz_error wz_header_from(
        struct wz* f,
        struct wz_header* header,
        uint8_t** off) {
    CHECK(_lenstr_at(f, header->ident, off, 4));
    CHECK(_uint64_t_at(f, &header->file_size, off));
    CHECK(_uint32_t_at(f, &header->file_start, off));
    CHECK(_nullstr_at(f, &header->copyright, off));

    return _NO_ERROR;
}

struct wz_error wz_open(
        struct wz* f,
        const char* filename) {
    int ret = 0;

    ret = _wz_openfileforread(
            &f->fd,
            &f->file_size,
            filename);
    if (ret) {
        struct wz_error err = {.kind = WZ_ERROR_KIND_ERRNO, .errno_ = ret};
        return err;
    }

    void* addr;
    ret = _wz_mapfile(
            &addr,
            f->fd,
            f->file_size);
    if (ret) {
        _wz_closefile(f->fd);

        struct wz_error err = {.kind = WZ_ERROR_KIND_ERRNO, .errno_ = ret};
        return err;
    }

    return wz_openfrom(f, addr);
}

struct wz_error wz_openfrom(
        struct wz* f,
        void* addr_v) {
    f->file_addr = (uint8_t*) addr_v;

    uint8_t* header_addr = f->file_addr;
    CHECK(wz_header_from(f, &f->header, &header_addr));

    uint8_t* root_addr = f->file_addr + f->header.file_start;
    CHECK(_uint16_t_at(f, &f->header.version, &root_addr));

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

    CHECK(wz_directory_from(f, &f->root, &root_addr));

    return _NO_ERROR;
}

struct wz_error wz_close(
        struct wz* f) {
    struct wz_error err = {0};

    if (f->fd) {
        int ret = 0;

        ret = _wz_unmapfile(f->file_addr, f->file_size);
        if (ret) {
            err.kind = WZ_ERROR_KIND_ERRNO;
            err.errno_ = errno;
        }

        ret = _wz_closefile(f->fd);
        if (ret) {
            err.kind = WZ_ERROR_KIND_ERRNO;
            err.errno_ = errno;
        }
    }

    return err;
}
