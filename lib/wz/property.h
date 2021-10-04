#ifndef WZ_PROPERTY_H
#define WZ_PROPERTY_H

#include <stdint.h>
#include <wchar.h>

struct wz;
struct wz_file;

enum wz_encryptedstring_kind {
    WZ_ENCRYPTEDSTRING_KIND_ONEBYTE,
    WZ_ENCRYPTEDSTRING_KIND_TWOBYTE,
};

struct wz_encryptedstring {
    enum wz_encryptedstring_kind kind;
    uint8_t* at;
    uint32_t len;
};

struct error wz_encryptedstring_from(
        const struct wz* f,
        struct wz_encryptedstring* s,
        uint8_t** off);

// wz_encryptedstring_decrypt decrypts the provided wz_encryptedstring into a
// provided buffer. The out buffer should be at least s->len wchar_t's in size.
struct error wz_encryptedstring_decrypt(
        const struct wz* f,
        const struct wz_encryptedstring* s,
        wchar_t* out);

struct wz_propertycontainer {
    uint32_t count;
    uint8_t* first;
};

struct error wz_propertycontainer_from(
        const struct wz* wz,
        struct wz_propertycontainer* c,
        uint8_t** off);

struct wz_namedpropertycontainer {
    uint32_t count;
    uint8_t* first;
};

struct wz_image {
    uint32_t width;
    uint32_t height;
    int32_t format;
    uint8_t format2;
    uint32_t unknown1;
    uint32_t length;
    uint8_t unknown2;

    uint8_t* data;
};

// wz_image_rawsize returns the uncompressed, unencrypted size of this image's data.
// Buffers passed to wz_image_pixels should be at least this size, in bytes.
uint32_t wz_image_rawsize(
        const struct wz_image* img);

// wz_image_pixels decodes the image data pointed to by img, and places the pixels
// into out, in RGBA8 format; that is: 1 byte r, 1 byte g, 1 byte b, 1 byte a.
struct error wz_image_pixels(
        const struct wz* wz,
        const struct wz_image* img,
        uint8_t* out);

struct wz_canvas {
    struct wz_propertycontainer children;
    struct wz_image image;
};

enum wz_property_kind {
    WZ_PROPERTY_KIND_UNSPECIFIED,
    WZ_PROPERTY_KIND_NULL,
    WZ_PROPERTY_KIND_UINT16,
    WZ_PROPERTY_KIND_INT32,
    WZ_PROPERTY_KIND_FLOAT32,
    WZ_PROPERTY_KIND_FLOAT64,
    WZ_PROPERTY_KIND_STRING,
    WZ_PROPERTY_KIND_CONTAINER,
    WZ_PROPERTY_KIND_NAMEDCONTAINER,
    WZ_PROPERTY_KIND_CANVAS,
    WZ_PROPERTY_KIND_VECTOR,
    WZ_PROPERTY_KIND_SOUND,
    WZ_PROPERTY_KIND_UOL,
};

struct wz_property {
    enum wz_property_kind kind;
    struct wz_encryptedstring name;

    union {
        uint16_t uint16;
        int32_t int32;
        float float32;
        double float64;
        struct wz_encryptedstring string;
        struct wz_propertycontainer container;
        struct wz_namedpropertycontainer named_container;
        struct wz_canvas canvas;
        int32_t vector[2];
        uint8_t* sound;
        struct wz_encryptedstring uol;
    };
};

// wz_property_namedfrom populates the property from offset, using the special
// named property logic. The provided offset is incremented along the way.
struct error wz_property_namedfrom(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off);

// wz_property_from populates the property from offset, incrementing
// it along the way.
struct error wz_property_from(
        const struct wz* wz,
        const struct wz_file* f,
        struct wz_property* p,
        uint8_t** off);

#endif
