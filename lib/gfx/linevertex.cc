#include "gfx/linevertex.hh"

namespace gfx {

Error LineVertex::configure(
    const gl::Program<LineVertex>* program,
    const gl::Drawable<LineVertex>* drawable) {
    glBindVertexArray(drawable->vao);
    glBindBuffer(GL_ARRAY_BUFFER, drawable->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable->ebo);

    GLuint position = 0;
    CHECK(program->attribute(
        "position",
        &position),
        Error::GLERROR)
        << "failed to configure program: no position attribute";

    GLuint color = 0;
    CHECK(program->attribute(
        "color",
        &color),
        Error::GLERROR)
        << "failed to configure program: no color attribute";

    glEnableVertexAttribArray(position);
    glEnableVertexAttribArray(color);

    GLsizei stride = sizeof(LineVertex);
    size_t position_offset = offsetof(LineVertex, position);
    size_t color_offset = offsetof(LineVertex, color);

    glVertexAttribPointer(
        position,
        2,
        GL_FLOAT,
        GL_FALSE,
        stride,
        reinterpret_cast<void*>(position_offset));
    glVertexAttribPointer(
        color,
        4,
        GL_FLOAT,
        GL_FALSE,
        stride,
        reinterpret_cast<void*>(color_offset));

    return Error();
}

}
