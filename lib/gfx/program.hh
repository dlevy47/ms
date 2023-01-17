#pragma once

#include "gfx/rect.hh"
#include "gfx/vertex.hh"

namespace gfx {

struct Program {
        gl::Program<Vertex> program;

        static Error init(
                Program* self);

        void viewport(
                gfx::Rect<double> viewport) {
                glUseProgram(program.program);
                glUniform2f(
                        program.uniforms.at("viewport_topleft"),
                        viewport.topleft.x,
                        viewport.topleft.y);
                glUniform2f(
                        program.uniforms.at("viewport_bottomright"),
                        viewport.bottomright.x,
                        viewport.bottomright.y);
        }

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

}
