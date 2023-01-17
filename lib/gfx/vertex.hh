#pragma once

#include "gl.hh"

namespace gfx {

struct Vertex {
    float position[2];
    float uv[2];

    static Error configure(
        const gl::Program<Vertex>* program,
        const gl::Drawable<Vertex>* drawable);
};

typedef gl::Drawable<Vertex> Drawable;

}
