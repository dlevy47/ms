#include "util/error.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

int error_frame_printto(const struct error_frame* e, FILE* outf) {
    int count = 0;
    int ret = 0;

    ret = fprintf(outf, "%s: ", error_kind_str(e->kind));
    if (ret < 0) return ret;
    count += ret;

    switch (e->kind) {
        case ERROR_KIND_NONE:
            break;
        case ERROR_KIND_BADOFFSET:
            {
                ret = fprintf(outf, "from: %p at: %p", e->bad_offset.from, e->bad_offset.at);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_BADREAD:
            {
                ret = fprintf(outf, "bad read");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_ERRNO:
            {
                ret = fprintf(outf, "errno %d: %s", e->errno_, strerror(e->errno_));
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_UNKNOWNDIRECTORYENTRYKIND:
            {
                ret = fprintf(outf, "unknown directory entry kind %d", e->unknown_directoryentry_kind);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_FILEPARSEERROR1:
            {
                ret = fprintf(outf, "failed to parse file: mismatch name location");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_FILEPARSEERROR2:
            {
                ret = fprintf(outf, "failed to parse file: name mismatch");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_FILEPARSEERROR3:
            {
                ret = fprintf(outf, "failed to parse file: 3");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_UNKNOWNSTRINGOFFSETKIND:
            {
                ret = fprintf(outf, "unknown string offset kind: %d", e->unknown_string_offset_kind);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_UNKNOWNPROPERTYKIND:
            {
                ret = fprintf(outf, "unknown property kind: %d", e->unknown_property_kind);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_UNKNOWNPROPERTYKINDNAME:
            {
                ret = fprintf(outf, "unknown property kind name");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_UNKNOWNIMAGEFORMAT:
            {
                ret = fprintf(outf, "unknown image format: %u %u", e->unknown_image_format.format, e->unknown_image_format.format2);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_DECOMPRESSIONFAILED:
            {
                ret = fprintf(outf, "zlib decompression failed: %d", e->decompression_failed);
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_FILEOPENFAILED:
            {
                ret = fprintf(outf, "file open failed");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_CLOSEFAILED:
            {
                ret = fprintf(outf, "close failed");
                if (ret < 0) return ret;
                count += ret;
            } break;
        case ERROR_KIND_VFSOPENFAILED:
            {
                ret = fprintf(outf, "vfs open failed");
                if (ret < 0) return ret;
                count += ret;
            } break;
    }

    ret = fprintf(outf, " (%ls)", e->message);
    if (ret < 0 ) return ret;
    count += ret;

    return count;
}

int error_printto(
    const struct error* e,
    FILE* outf) {
    if (e->next_frame == 0) {
        return fprintf(outf, "no error");
    }

    int count = 0;
    for (size_t i = e->next_frame - 1; ; --i) {
        int ret = error_frame_printto(e->frames + i, outf);
        if (ret < 0) return ret;
        count += ret;

        if (i == 0)
            break;
    }

    return count;
}

struct error_frame* error_vpush(
    struct error* e,
    const char* file,
    size_t line,
    enum error_kind kind,
    const wchar_t* message_format,
    va_list ap) {
    if (e->next_frame >= e->frames_cap) {
        e->frames_cap += 16;
        e->frames = realloc(e->frames, sizeof(*e->frames) * e->frames_cap);
    }

    struct error_frame* frame = e->frames + e->next_frame;
    frame->file = file;
    frame->line = line;
    frame->kind = kind;
    vswprintf(frame->message, error_message_len, message_format, ap);
    ++e->next_frame;

    return frame;
}
