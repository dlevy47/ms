#pragma once

#include "gl.hh"

namespace gfx {

struct LineVertex {
    float position[2];
    float color[4];

    static Error configure(
            const gl::Program<LineVertex>* program,
            const gl::Drawable<LineVertex>* drawable);
};

}
