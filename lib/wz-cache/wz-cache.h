#ifndef INCLUDED_WZ_CACHE_WZ_CACHE_H
#define INCLUDED_WZ_CACHE_WZ_CACHE_H

#include "wz/wz.h"

struct wz_cache;

enum wz_cache_node_kind {
    WZ_CACHE_NODE_KIND_DIRECTORY,
    WZ_CACHE_NODE_KIND_FILE,
    WZ_CACHE_NODE_KIND_PROPERTY,
};

struct wz_cache_node {
    struct wz_cache* cache;

    // parent may be NULL in the case that this is the root node.
    struct wz_cache_node* parent;

    // prev is NULL if this is the first node.
    struct wz_cache_node* prev;

    // next is NULL if this is the last node.
    struct wz_cache_node* next;

    wchar_t* name;

    enum wz_cache_node_kind kind;
    union {
        struct wz_directory directory;
        struct wz_file file;
        struct {
            struct wz_file* containing_file;
            struct wz_property property;
        } property;
    };

    // children is a linked list of child nodes.
    struct wz_cache_node* children;

    // TODO: add name index.
};

struct wz_error wz_cache_node_find(
        struct wz_cache_node** out,
        struct wz_cache_node* node,
        const wchar_t* name);

struct wz_cache {
    struct wz wz;

    struct wz_cache_node root;
};

void wz_cache_init(
        struct wz_cache* cache,
        struct wz wz);

struct wz_error wz_cache_free(
        struct wz_cache* cache);

static inline struct wz_error wz_cache_find(
        struct wz_cache_node** out,
        struct wz_cache* cache,
        const wchar_t* name) {
    return wz_cache_node_find(out, &cache->root, name);
}

#endif
