#pragma once

#include <memory>

#include "nuklear/nuklear.h"
#include "util/error.hh"

namespace nk {

// Convenience typedefs.
typedef struct nk_convert_config ConvertConfig;
typedef struct nk_draw_null_texture DrawNullTexture;

template <typename T, void (*F)(T*)>
struct Handle {
    T x;
    bool present;

    Handle(): x(), present(false) {}
    Handle(T x): x(x), present(true) {}

    void set(T from) {
        x = from;
        present = true;
    }

    operator T* () { return &x; }
    operator const T* () const { return &x; }

    T* operator->() { return &x; }
    const T* operator->() const { return &x; }

    Handle(Handle&& rhs):
        x(),
        present(std::exchange(rhs.present, false)) {
        x = rhs.x;
    }

    ~Handle() {
        if (present && F)
            F(&x);
    }

    Handle(const Handle&) = delete;
};

typedef Handle<struct nk_font_atlas, nk_font_atlas_clear> FontAtlas;
typedef Handle<struct nk_context, nk_free> Context;

inline void* Allocator_alloc(nk_handle, void* old, nk_size size) {
    return ::calloc(1, size);
}

inline void Allocator_free(nk_handle, void* old) {
    ::free(old);
}

// Allocator is an nk_allocator that zeroes allocated memory.
static struct nk_allocator Allocator = {
    .alloc = Allocator_alloc,
    .free = Allocator_free,
};

}
