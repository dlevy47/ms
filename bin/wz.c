#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wz/wz.h"
#include "wz-cache/wz-cache.h"

#include "lodepng/lodepng.h"

static void print_indent(
        size_t indent,
        FILE* outf) {
    for (size_t i = 0; i < indent; ++i) {
        fprintf(outf, " ");
    }
}

static int print_propertycontainer(
        const struct wz* wz,
        const struct wz_file* f,
        const struct wz_propertycontainer* c,
        size_t indent);
static int print_namedpropertycontainer(
        const struct wz* wz,
        const struct wz_file* f,
        const struct wz_namedpropertycontainer* c,
        size_t indent);

static int print_property(
        const struct wz* wz,
        const struct wz_file* f,
        const struct wz_property* property,
        size_t indent) {
    struct wz_error err = {0};

    print_indent(indent, stdout);

    wchar_t* name = calloc(sizeof(*name), property->name.len + 1);
    err = wz_encryptedstring_decrypt(wz, &property->name, name);
    if (err.kind) {
        fprintf(stderr, "failed to decrypt name of property entry: ");
        wz_error_printto(&err, stderr);
        fprintf(stderr, "\n");
        return 1;
    }
    printf("%ls = ", name);
    free(name);

    switch (property->kind) {
        case WZ_PROPERTY_KIND_NULL:
            {
                printf("NULL\n");
            } break;
        case WZ_PROPERTY_KIND_UINT16:
            {
                printf("[u16] %hu\n", property->uint16);
            } break;
        case WZ_PROPERTY_KIND_UINT32:
            {
                printf("[u32] %u\n", property->uint32);
            } break;
        case WZ_PROPERTY_KIND_FLOAT32:
            {
                printf("[f32] %f\n", property->float32);
            } break;
        case WZ_PROPERTY_KIND_FLOAT64:
            {
                printf("[f64] %lf\n", property->float64);
            } break;
        case WZ_PROPERTY_KIND_STRING:
            {
                wchar_t* s = calloc(sizeof(*s), property->string.len + 1);

                err = wz_encryptedstring_decrypt(wz, &property->string, s);
                if (err.kind) {
                    fprintf(stderr, "failed to decrypt string property: ");
                    wz_error_printto(&err, stderr);
                    fprintf(stderr, "\n");
                    return 1;
                }

                printf("[string] \"%ls\"\n", s);

                free(s);
            } break;
        case WZ_PROPERTY_KIND_CONTAINER:
            {
                printf("[container]\n");
                if (print_propertycontainer(wz, f, &property->container, indent + 2)) {
                    return 1;
                }
            } break;
        case WZ_PROPERTY_KIND_NAMEDCONTAINER:
            {
                printf("[named container]\n");
                if (print_namedpropertycontainer(wz, f, &property->named_container, indent + 2)) {
                    return 1;
                }
            } break;
        case WZ_PROPERTY_KIND_CANVAS:
            {
                int encrypted = (property->canvas.image.data[0] == 0x78 &&
                        property->canvas.image.data[1] == 0x9C);

                printf("[canvas] %u bytes@%p (%s) %ux%u (format %u %u)\n",
                        property->canvas.image.length,
                        property->canvas.image.data,
                        (encrypted ? "encrypted" : "not encrypted"),
                        property->canvas.image.width,
                        property->canvas.image.height,
                        property->canvas.image.format,
                        property->canvas.image.format2);
                if (print_propertycontainer(wz, f, &property->canvas.children, indent + 2)) {
                    return 1;
                }

                {
                    static int image_index = 0;
                    char filename[64] = {0};
                    snprintf(filename, sizeof(filename), "%d.png", image_index);
                    ++image_index;

                    uint8_t* image = calloc(sizeof(*image), wz_image_rawsize(&property->canvas.image));

                    err = wz_image_pixels(wz, &property->canvas.image, image);
                    if (err.kind) {
                        free(image);
                        fprintf(stderr, "failed to get image pixels: ");
                        wz_error_printto(&err, stderr);
                        fprintf(stderr, "\n");
                        return 1;
                    }

                    lodepng_encode32_file(filename, image, property->canvas.image.width, property->canvas.image.height);
                    free(image);
                }
            } break;
        case WZ_PROPERTY_KIND_VECTOR:
            {
                printf("[vector] %u, %u\n", property->vector[0], property->vector[1]);
            } break;
        case WZ_PROPERTY_KIND_SOUND:
            {
                printf("[sound] @%p\n", property->sound);
            } break;
        case WZ_PROPERTY_KIND_UOL:
            {
                wchar_t* s = calloc(sizeof(*s), property->string.len + 1);

                err = wz_encryptedstring_decrypt(wz, &property->string, s);
                if (err.kind) {
                    fprintf(stderr, "failed to decrypt uol property: ");
                    wz_error_printto(&err, stderr);
                    fprintf(stderr, "\n");
                    return 1;
                }

                printf("[uol] \"%ls\"\n", s);

                free(s);
            } break;
        case WZ_PROPERTY_KIND_UNSPECIFIED:
            break;
    }

    return 0;
}

static int print_propertycontainer(
        const struct wz* wz,
        const struct wz_file* f,
        const struct wz_propertycontainer* c,
        size_t indent) {
    uint8_t* property_addr = c->first;
    for (uint32_t i = 0; i < c->count; ++i) {
        struct wz_error err = {0};

        struct wz_property property = {0};
        err = wz_property_from(wz, f, &property, &property_addr);
        if (err.kind) {
            fprintf(stderr, "failed to read property entry %d: ", i);
            wz_error_printto(&err, stderr);
            fprintf(stderr, "\n");
            return 1;
        }

        if (print_property(wz, f, &property, indent)) {
            return 1;
        }
    }

    return 0;
}

static int print_namedpropertycontainer(
        const struct wz* wz,
        const struct wz_file* f,
        const struct wz_namedpropertycontainer* c,
        size_t indent) {
    uint8_t* property_addr = c->first;
    for (uint32_t i = 0; i < c->count; ++i) {
        struct wz_error err = {0};

        struct wz_property property = {0};
        err = wz_property_namedfrom(wz, f, &property, &property_addr);
        if (err.kind) {
            fprintf(stderr, "failed to read property entry %d: ", i);
            wz_error_printto(&err, stderr);
            fprintf(stderr, "\n");
            return 1;
        }

        if (print_property(wz, f, &property, indent)) {
            return 1;
        }
    }

    return 0;
}

static int print_directory(
        const struct wz* wz,
        const struct wz_directory* dir,
        size_t indent) {
    uint8_t* entry_addr = dir->first_entry;
    for (uint32_t i = 0; i < dir->count; ++i) {
        struct wz_error err = {0};

        struct wz_directoryentry entry = {0};
        err = wz_directoryentry_from(wz, &entry, &entry_addr);
        if (err.kind) {
            fprintf(stderr, "failed to read directory entry %d: ", i);
            wz_error_printto(&err, stderr);
            fprintf(stderr, "\n");
            return 1;
        }

        switch (entry.kind) {
            case WZ_DIRECTORYENTRY_KIND_UNKNOWN:
                print_indent(indent, stdout);
                printf("%u %hu %u\n", entry.unknown.unknown1, entry.unknown.unknown2, entry.unknown.offset);
                break;
            case WZ_DIRECTORYENTRY_KIND_FILE:
                {
                    wchar_t* name = calloc(sizeof(*name), entry.file.name.len + 1);

                    err = wz_encryptedstring_decrypt(wz, &entry.file.name, name);
                    if (err.kind) {
                        fprintf(stderr, "failed to decrypt name of directory entry %d: ", i);
                        wz_error_printto(&err, stderr);
                        fprintf(stderr, "\n");
                        return 1;
                    }

                    print_indent(indent, stdout);
                    printf("* %ls\n", name);

                    if (print_propertycontainer(wz, &entry.file.file, &entry.file.file.root, indent + 2)) {
                        return 1;
                    }

                    free(name);
                } break;
            case WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY:
                {
                    wchar_t* name = calloc(sizeof(*name), entry.subdirectory.name.len + 1);

                    err = wz_encryptedstring_decrypt(wz, &entry.subdirectory.name, name);
                    if (err.kind) {
                        fprintf(stderr, "failed to decrypt name of directory entry %d: ", i);
                        wz_error_printto(&err, stderr);
                        fprintf(stderr, "\n");
                        return 1;
                    }

                    print_indent(indent, stdout);
                    printf("%ls/\n", name);

                    free(name);
                    if (print_directory(
                                wz,
                                &entry.subdirectory.subdirectory,
                                indent + 2)) {
                        return 1;
                    }
                } break;
            default:
                printf("\n");
                break;
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s filename path\n", argv[0]);
        return 1;
    }

    struct wz_error err = {0};

    struct wz wz = {0};
    err = wz_open(&wz, argv[1]);
    if (err.kind) {
        fprintf(stderr, "failed to open file: ");
        wz_error_printto(&err, stderr);
        fprintf(stderr, "\n");
        return 1;
    }

    struct wz_cache cache = {0};
    wz_cache_init(&cache, wz);

    printf("(@%p) %s: %s, v%d\n", cache.wz.file_addr, cache.wz.header.ident, cache.wz.header.copyright, cache.wz.version);
    printf("  file_size: %zu\n", cache.wz.header.file_size);
    printf("  file_start: %u\n", cache.wz.header.file_start);
    printf("  version_hash: %u\n", cache.wz.header.version_hash);

    wchar_t* path = calloc(strlen(argv[2]) + 1, sizeof(*path));
    swprintf(path, strlen(argv[2]) + 1, L"%s", argv[2]);

    struct wz_cache_node* needle = NULL;
    err = wz_cache_find(
            &needle,
            &cache,
            path);
    free(path);
    if (err.kind) {
        fprintf(stderr, "failed to find needle %ls: ", path);
        wz_error_printto(&err, stderr);
        fprintf(stderr, "\n");
        return 1;
    }

    if (needle) {
        switch (needle->kind) {
            case WZ_CACHE_NODE_KIND_DIRECTORY:
                print_directory(&cache.wz, &needle->directory, 0);
                break;
            case WZ_CACHE_NODE_KIND_FILE:
                print_propertycontainer(&cache.wz, &needle->file, &needle->file.root, 0);
                break;
            case WZ_CACHE_NODE_KIND_PROPERTY:
                print_property(&cache.wz, needle->property.containing_file, &needle->property.property, 0);
                break;
        }
    } else {
        printf("path not found\n");
    }

    err = wz_cache_free(&cache);
    if (err.kind) {
        fprintf(stderr, "failed to free cache: ");
        wz_error_printto(&err, stderr);
        fprintf(stderr, "\n");
        return 1;
    }

    return 0;
}
