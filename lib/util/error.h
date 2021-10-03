#ifndef UTIL_ERROR_H
#define UTIL_ERROR_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum error_kind {
    ERROR_KIND_NONE,
    ERROR_KIND_BADREAD,
    ERROR_KIND_BADOFFSET,
    ERROR_KIND_ERRNO,
    ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND,
    ERROR_KIND_FILEPARSEERROR1,
    ERROR_KIND_FILEPARSEERROR2,
    ERROR_KIND_FILEPARSEERROR3,
    ERROR_KIND_UNKNOWNSTRINGOFFSETKIND,
    ERROR_KIND_UNKNOWNPROPERTYKIND,
    ERROR_KIND_UNKNOWNPROPERTYKINDNAME,
    ERROR_KIND_UNKNOWNIMAGEFORMAT,
    ERROR_KIND_DECOMPRESSIONFAILED,
    ERROR_KIND_FILEOPENFAILED,
    ERROR_KIND_CLOSEFAILED,
    ERROR_KIND_VFSOPENFAILED,
};

static inline const char* error_kind_str(
        enum error_kind k) {
    switch (k) {
        case ERROR_KIND_NONE:
            return "ERROR_KIND_NONE";
        case ERROR_KIND_BADREAD:
            return "ERROR_KIND_BADREAD";
        case ERROR_KIND_BADOFFSET:
            return "ERROR_KIND_BADOFFSET";
        case ERROR_KIND_ERRNO:
            return "ERROR_KIND_ERRNO";
        case ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND:
            return "ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND";
        case ERROR_KIND_FILEPARSEERROR1:
            return "ERROR_KIND_FILEPARSEERROR1";
        case ERROR_KIND_FILEPARSEERROR2:
            return "ERROR_KIND_FILEPARSEERROR2";
        case ERROR_KIND_FILEPARSEERROR3:
            return "ERROR_KIND_FILEPARSEERROR3";
        case ERROR_KIND_UNKNOWNSTRINGOFFSETKIND:
            return "ERROR_KIND_UNKNOWNSTRINGOFFSETKIND";
        case ERROR_KIND_UNKNOWNPROPERTYKIND:
            return "ERROR_KIND_UNKNOWNPROPERTYKIND";
        case ERROR_KIND_UNKNOWNPROPERTYKINDNAME:
            return "ERROR_KIND_UNKNOWNPROPERTYKINDNAME";
        case ERROR_KIND_UNKNOWNIMAGEFORMAT:
            return "ERROR_KIND_UNKNOWNIMAGEFORMAT";
        case ERROR_KIND_DECOMPRESSIONFAILED:
            return "ERROR_KIND_DECOMPRESSIONFAILED";
        case ERROR_KIND_FILEOPENFAILED:
            return "ERROR_KIND_FILEOPENFAILED";
        case ERROR_KIND_CLOSEFAILED:
            return "ERROR_KIND_CLOSEFAILED";
        case ERROR_KIND_VFSOPENFAILED:
            return "ERROR_KIND_VFSOPENFAILED";
        default:
            return "ERROR_KIND_??";
    }
}

enum { error_message_len = 1023 };

struct error_frame {
	const char* file;
	size_t line;
	wchar_t message[error_message_len + 1];

    enum error_kind kind;

    union {
        struct {
            uint8_t* from;
            uint8_t* at;
        } bad_offset;

        int errno_;

        uint8_t unknown_directoryentry_kind;
        uint8_t unknown_string_offset_kind;
        uint8_t unknown_property_kind;
        struct {
            uint32_t format;
            uint8_t format2;
        } unknown_image_format;

        int decompression_failed;
    };
};

int error_frame_printto(
	const struct error_frame* e,
	FILE* outf);

struct error {
	struct error_frame* frames;
	size_t next_frame;
    size_t frames_cap;
};

int error_printto(
    const struct error* e,
    FILE* outf);

static inline struct error_frame* error_top(struct error* e) {
	if (e->next_frame == 0) return NULL;
	return e->frames + e->next_frame - 1;
}

struct error_frame* error_vpush(
	struct error* e,
	const char* file,
	size_t line,
	enum error_kind kind,
	const wchar_t* message_format,
	va_list ap);

static inline struct error_frame* error_pushex(
	struct error* e,
	const char* file,
	size_t line,
	enum error_kind kind,
	const wchar_t* message_format,
	...) {
	va_list ap;
	va_start(ap, message_format);

	struct error_frame* top = error_vpush(e, file, line, kind, message_format, ap);

	va_end(ap);
	return top;
}

#define error_push(e, k, ...) error_pushex(e, __FILE__, __LINE__, k, __VA_ARGS__)

static inline struct error error_newex(
	const char* file,
	size_t line,
	enum error_kind kind,
	const wchar_t* message_format,
	...) {
	va_list ap;
	va_start(ap, message_format);

	struct error e = {0};
	error_vpush(&e, file, line, kind, message_format, ap);

	va_end(ap);
	return e;
}

#define error_new(k, ...) error_newex(__FILE__, __LINE__, k, __VA_ARGS__)

static inline void error_free(struct error* e) {
    free(e->frames);
}

#define CHECK(x, k, ...) do { \
    struct error err = (x); \
    if (error_top(&err)) { \
        error_pushex(&err, __FILE__, __LINE__, k, __VA_ARGS__); \
        return err; \
    } \
} while (0)

#endif