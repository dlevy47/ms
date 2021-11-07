#pragma once

#include "util/error.hh"
#include "wz/directory.hh"
#include "wz/property.hh"
#include "wz/wz.hh"

namespace wz {

struct Vfs {
};

struct OpenedFile {
    struct Node {
        Property::Kind kind;

        // parent is the index of this node's parent in the containing OpenedFile's
        // nodes arena.
        uint32_t parent;

        // name is a pointer to the null-terminated name of this node, internal
        // to the containing OpenedFile's strings arena.
        wchar_t* name;

        // children is a span of node indices in the containing OpenedFile's children
        // arena.
        struct {
            uint32_t count;
            uint32_t* start;
        } children;

        union {
            uint16_t uint16;
            int32_t int32;
            float float32;
            double float64;
            wchar_t* string;
            int32_t vector[2];
            uint8_t* sound;
            wchar_t* uol;
            struct {
                wz::Image image;
                uint8_t* image_data;
            } canvas;
        };
    };

    // strings is an arena containing null-terminated decrypted strings in this file.
    wchar_t* strings;

    // images is an arena containing decompressed image information. This is not
    // an atlas; image data here is one after the other.
    uint8_t* images;

    // children is an arena containing spans of child indices for nodes in this file.
    uint32_t* children;

    // nodes is an arena containing the Nodes in this file.
    Node* nodes;

    static Error open(
            const wz::Wz* wz,
            OpenedFile* of,
            const wz::File* f);

    OpenedFile():
        strings(nullptr),
        images(nullptr),
        children(nullptr),
        nodes(nullptr) {}

    ~OpenedFile() {
        delete[] strings;
        delete[] images;
        delete[] children;
        delete[] nodes;
    }

    OpenedFile(const OpenedFile&) = delete;
};

}
