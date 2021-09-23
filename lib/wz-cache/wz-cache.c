#include "wz-cache/wz-cache.h"
#include "wz/wz-internal.h"

static int limited_wcsncmp(
        const wchar_t* name,
        const wchar_t* right,
        uint32_t name_len) {
    uint32_t i = 0;
    for (i = 0; *right && i < name_len; ++i) {
        if (name[i] != *right) {
            return 1;
        }
        ++right;
    }

    // Only consider these equal if we've reached the ends.
    if (i == name_len && *right == 0)
        return 0;
    return 1;
}

static struct wz_error wz_cache_node_findinpropertycontainer(
        struct wz_cache_node** out,
        struct wz_cache_node* node,
        const wchar_t* name,
        uint32_t name_len,
        struct wz_file* containing_file,
        struct wz_propertycontainer* container) {
    uint8_t* property_addr = container->first;
    for (uint32_t i = 0; i < container->count; ++i) {
        struct wz_property property = {0};
        CHECK(wz_property_from(&node->cache->wz, containing_file, &property, &property_addr));

        wchar_t* child_name = calloc(sizeof(*child_name), property.name.len + 1);
        struct wz_error err = wz_encryptedstring_decrypt(&node->cache->wz, &property.name, child_name);
        if (err.kind) {
            free(child_name);
            return err;
        }

        struct wz_cache_node* child = calloc(1, sizeof(*child));
        child->cache = node->cache;
        child->parent = node;
        child->next = node->children;
        if (node->children) {
            node->children->prev = child;
        }
        node->children = child;
        child->name = child_name;

        child->kind = WZ_CACHE_NODE_KIND_PROPERTY;
        child->property.containing_file = containing_file;
        child->property.property = property;

        if (limited_wcsncmp(name, child->name, name_len) == 0) {
            return wz_cache_node_find(out, child, name + name_len);
        }
    }

    struct wz_error err = {0};
    return err;
}

static struct wz_error wz_cache_node_findinnamedpropertycontainer(
        struct wz_cache_node** out,
        struct wz_cache_node* node,
        const wchar_t* name,
        uint32_t name_len,
        struct wz_file* containing_file,
        struct wz_namedpropertycontainer* container) {
    uint8_t* property_addr = container->first;
    for (uint32_t i = 0; i < container->count; ++i) {
        struct wz_property property = {0};
        CHECK(wz_property_namedfrom(&node->cache->wz, containing_file, &property, &property_addr));

        wchar_t* child_name = calloc(sizeof(*child_name), property.name.len + 1);
        struct wz_error err = wz_encryptedstring_decrypt(&node->cache->wz, &property.name, child_name);
        if (err.kind) {
            free(child_name);
            return err;
        }

        struct wz_cache_node* child = calloc(1, sizeof(*child));
        child->cache = node->cache;
        child->parent = node;
        child->next = node->children;
        if (node->children) {
            node->children->prev = child;
        }
        node->children = child;
        child->name = child_name;

        child->kind = WZ_CACHE_NODE_KIND_PROPERTY;
        child->property.containing_file = containing_file;
        child->property.property = property;

        if (limited_wcsncmp(name, child->name, name_len) == 0) {
            return wz_cache_node_find(out, child, name + name_len);
        }
    }

    struct wz_error err = {0};
    return err;
}

struct wz_error wz_cache_node_find(
        struct wz_cache_node** out,
        struct wz_cache_node* node,
        const wchar_t* name) {
    // Base case: empty name. In this case, return node.
    if (name[0] == 0) {
        *out = node;
        struct wz_error err = {0};
        return err;
    }

    // Janky fix.
    while (*name == L'/') {
        ++name;
    }

    // First, determine the end of name (it'll be either the null terminator, or the first /).
    uint32_t name_len = 0;
    while (name[name_len] != 0 && name[name_len] != L'/') {
        ++name_len;
    }

    // Special cases: ".." ascends, and "." returns this node.
    if (limited_wcsncmp(name, L"..", name_len) == 0) {
        if (node->parent == NULL) {
            struct wz_error err = {0};
            *out = NULL;
            return err;
        }

        return wz_cache_node_find(out, node->parent, name + name_len);
    } else if (limited_wcsncmp(name, L".", name_len) == 0) {
        struct wz_error err = {0};
        *out = node;
        return err;
    }

    // Then, see if we already have a node with the specified name.
    // TODO: use index here instead of naive search.
    struct wz_cache_node* cur = node->children;
    while (cur) {
        if (limited_wcsncmp(name, cur->name, name_len) == 0) {
            // Yep, so descend.
            return wz_cache_node_find(out, cur, name + name_len);
        }

        cur = cur->next;
    }

    // We didn't find a child with this name. Look in the file for a child by this name.
    switch (node->kind) {
        case WZ_CACHE_NODE_KIND_DIRECTORY:
            {
                uint8_t* entry_addr = node->directory.first_entry;
                for (uint32_t i = 0; i < node->directory.count; ++i) {
                    struct wz_directoryentry entry = {0};
                    CHECK(wz_directoryentry_from(&node->cache->wz, &entry, &entry_addr));

                    struct wz_cache_node* child = NULL;
                    switch (entry.kind) {
                        case WZ_DIRECTORYENTRY_KIND_FILE:
                            {
                                wchar_t* child_name = calloc(sizeof(*child_name), entry.file.name.len + 1);

                                struct wz_error err = wz_encryptedstring_decrypt(&node->cache->wz, &entry.file.name, child_name);
                                if (err.kind) {
                                    free(child_name);
                                    return err;
                                }

                                child = calloc(1, sizeof(*child));
                                child->cache = node->cache;
                                child->parent = node;
                                child->next = node->children;
                                if (node->children) {
                                    node->children->prev = child;
                                }
                                node->children = child;
                                child->name = child_name;

                                child->kind = WZ_CACHE_NODE_KIND_FILE;
                                child->file = entry.file.file;
                            } break;
                        case WZ_DIRECTORYENTRY_KIND_SUBDIRECTORY:
                            {
                                wchar_t* child_name = calloc(sizeof(*child_name), entry.subdirectory.name.len + 1);

                                struct wz_error err = wz_encryptedstring_decrypt(&node->cache->wz, &entry.subdirectory.name, child_name);
                                if (err.kind) {
                                    free(child_name);
                                    return err;
                                }

                                child = calloc(1, sizeof(*child));
                                child->cache = node->cache;
                                child->parent = node;
                                child->next = node->children;
                                if (node->children) {
                                    node->children->prev = child;
                                }
                                node->children = child;
                                child->name = child_name;

                                child->kind = WZ_CACHE_NODE_KIND_DIRECTORY;
                                child->directory = entry.subdirectory.subdirectory;
                            } break;
                        default:
                            continue;
                    }

                    if (limited_wcsncmp(name, child->name, name_len) == 0) {
                        return wz_cache_node_find(out, child, name + name_len);
                    }
                }
            } break;
        case WZ_CACHE_NODE_KIND_FILE:
            return wz_cache_node_findinpropertycontainer(
                    out,
                    node,
                    name,
                    name_len,
                    &node->file,
                    &node->file.root);
        case WZ_CACHE_NODE_KIND_PROPERTY:
            {
                switch (node->property.property.kind) {
                    default:
                        {
                            struct wz_error err = {0};
                            *out = NULL;
                            return err;
                        } break;
                    // The following kinds of properties are the only ones with children.
                    case WZ_PROPERTY_KIND_CONTAINER:
                        return wz_cache_node_findinpropertycontainer(
                                out,
                                node,
                                name,
                                name_len,
                                node->property.containing_file,
                                &node->property.property.container);
                    case WZ_PROPERTY_KIND_NAMEDCONTAINER:
                        return wz_cache_node_findinnamedpropertycontainer(
                                out,
                                node,
                                name,
                                name_len,
                                node->property.containing_file,
                                &node->property.property.named_container);
                    case WZ_PROPERTY_KIND_CANVAS:
                        return wz_cache_node_findinpropertycontainer(
                                out,
                                node,
                                name,
                                name_len,
                                node->property.containing_file,
                                &node->property.property.canvas.children);
                }
            } break;
    }

    struct wz_error err = {0};
    return err;
}

void wz_cache_node_free(
        struct wz_cache_node* node) {
    // First, free name.
    free(node->name);

    // Then, free children:
    struct wz_cache_node* cur = node->children;
    while (cur) {
        wz_cache_node_free(cur);

        struct wz_cache_node* to_free = cur;
        cur = cur->next;
        free(to_free);
    }
}

void wz_cache_init(
        struct wz_cache* cache,
        struct wz wz) {
    cache->wz = wz;

    cache->root.cache = cache;
    cache->root.kind = WZ_CACHE_NODE_KIND_DIRECTORY;
    cache->root.directory = wz.root;
}

struct wz_error wz_cache_free(
        struct wz_cache* cache) {
    wz_cache_node_free(&cache->root);

    return wz_close(&cache->wz);
}
