#pragma once 

#include "gl.hh"
#include "nk.hh"
#include "gl/window.hh"
#include "util/error.hh"

namespace nk {

struct Vertex {
        float position[2];
        float uv[2];
        uint8_t color[4];

        static Error configure(
                const gl::Program<Vertex>* program,
                const gl::Drawable<Vertex>* drawable);
};

struct Program {
        gl::Program<Vertex> program;

        static Error init(
                Program* self);

        void projection(
                GLfloat projection[4][4]) {
                glUseProgram(program.program);
                glUniformMatrix4fv(
                        program.uniforms.at("projection"),
                        1,
                        GL_FALSE,
                        &projection[0][0]);
        }

        Program() = default;
        Program(Program&&) = default;
        Program(const Program&) = delete;
};

struct Ui {
        // font_atlas is the loaded nk font atlas. This must live for the entirety
        // of the context.
        nk::FontAtlas atlas;

        nk::DrawNullTexture null_texture;

        // nk_convert_config is used by nk to convert native draw commands to
        // vertex buffers.
        nk::ConvertConfig convert_config;

        // context is the nk context.
        nk::Context context;

        void input(
                gl::Window* window,
                bool keys);

        static Error init(
                Ui* self);
};

}
