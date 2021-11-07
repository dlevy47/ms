#include "client/ui/nkrenderer.hh"

namespace client {
namespace ui {

Error NkRenderer::init(
        NkRenderer* self) {
    CHECK(nk::Program::init(
                &self->program),
            Error::UIERROR) << "failed to initialize ui render program";

    gl::Drawable<nk::Vertex>::InitOptions init_options;
    CHECK(gl::Drawable<nk::Vertex>::init(
                &self->drawable,
                &self->program.program,
                &init_options),
            Error::UIERROR) << "failed to init drawable";

    return Error();
}

void NkRenderer::render(
        gl::Window::Frame* target,
        gfx::Vector<int32_t> window_size,
        nk::Ui* ui) {
    gfx::Vector<GLfloat> scale = (gfx::Vector<GLfloat>) window_size;
    GLfloat projection[4][4] = {{0}};

    projection[0][0] = 2 / scale.x;

    projection[1][1] = -2 / scale.y;

    projection[2][2] = 1;

    projection[3][0] = -1;
    projection[3][1] = 1;
    projection[3][3] = 1;

    program.projection(projection);

    struct nk_buffer commands = {0};
    struct nk_buffer vertices = {0};
    struct nk_buffer indices = {0};
    nk_buffer_init(&commands, &nk::Allocator, 4096);
    nk_buffer_init(&vertices, &nk::Allocator, 4096);
    nk_buffer_init(&indices, &nk::Allocator, 4096);
    nk_convert(ui->context, &commands, &vertices, &indices, &ui->convert_config);

    drawable.vbo_load(
            static_cast<nk::Vertex*>(nk_buffer_memory(&vertices)),
            vertices.size / sizeof(nk::Vertex));
    drawable.ebo_load(
            static_cast<uint16_t*>(nk_buffer_memory(&indices)),
            indices.size / sizeof(uint16_t));

    const nk_draw_index* offset = nullptr;
    const struct nk_draw_command* cmd = nullptr;
    nk_draw_foreach(cmd, ui->context, &commands) {
        if (!cmd->elem_count) continue;

        gfx::Vector<int32_t> scale = {1, 1};

        glBindTexture(GL_TEXTURE_2D, (GLuint) cmd->texture.id);
        glScissor(
                (GLint) (cmd->clip_rect.x * scale.x),
                (GLint) ((window_size.y - (GLint) (cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint) (cmd->clip_rect.w * scale.x),
                (GLint) (cmd->clip_rect.h * scale.y));

        glUseProgram(program.program);
        glBindVertexArray(drawable.vao);
        glBindBuffer(GL_ARRAY_BUFFER, drawable.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawable.ebo);

        glDrawElements(
                drawable.vbo_mode,
                (GLsizei) cmd->elem_count,
                drawable.ebo_type,
                offset);
        offset += cmd->elem_count;
    }

    nk_buffer_free(&commands);
    nk_buffer_free(&vertices);
    nk_buffer_free(&indices);

    nk_clear(ui->context);
}

}
}
