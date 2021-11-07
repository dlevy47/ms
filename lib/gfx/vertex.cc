#include "gfx/vertex.hh"

#include "gfx/linevertex.hh"

namespace gfx {

Error Vertex::configure(
        const gl::Program<Vertex>* program,
        const gl::Drawable<Vertex>* drawable) {
    glBindVertexArray(drawable->vao);
    glBindBuffer(GL_ARRAY_BUFFER, drawable->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->ebo);

    GLuint position = 0;
    CHECK(program->attribute(
                "position",
                &position),
            Error::GLERROR)
        << "failed to configure program";

    GLuint uv = 0;
    CHECK(program->attribute(
                "uv",
                &uv),
            Error::GLERROR)
        << "failed to configure program";

    glEnableVertexAttribArray(position);
    glEnableVertexAttribArray(uv);

    GLsizei stride = sizeof(Vertex);
    size_t position_offset = offsetof(Vertex, position);
    size_t uv_offset = offsetof(Vertex, uv);

    glVertexAttribPointer(
            position,
            2,
            GL_FLOAT,
            GL_FALSE,
            stride,
            reinterpret_cast<void*>(position_offset));
    glVertexAttribPointer(
            uv,
            2,
            GL_FLOAT,
            GL_FALSE,
            stride,
            reinterpret_cast<void*>(uv_offset));

    return Error();
}

}

#include "gl.impl.cc"

template struct gl::Program<gfx::Vertex>;
template struct gl::Drawable<gfx::Vertex>;

template struct gl::Program<gfx::LineVertex>;
template struct gl::Drawable<gfx::LineVertex>;
