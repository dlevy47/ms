#pragma once

#include "gl.hh"
#include "nk.hh"
#include "gfx/program.hh"
#include "gfx/rect.hh"
#include "gl/window.hh"
#include "nk/ui.hh"
#include "util/error.hh"

namespace client {
namespace ui {

struct NkRenderer {
    gl::Drawable<nk::Vertex> drawable;

    nk::Program program;

    static Error init(
            NkRenderer* self);

    void render(
            gl::Window::Frame* target,
            gfx::Vector<int32_t> window_size,
            nk::Ui* ui);

    NkRenderer() = default;
    NkRenderer(NkRenderer&&) = default;
    NkRenderer(const NkRenderer&) = delete;
};

}
}
