#include "gfx/program.hh"

#include "logger.hh"

namespace gfx {

Error Program::init(
    Program* self) {
    LOG(Logger::INFO) << "compiling shaders";
    gl::Program<Vertex>::CompileOptions compile_options;
    compile_options.vertex_shader =
        "#version 330\n"
        "uniform mat4 projection;\n"
        "in vec2 position;\n"
        "in vec2 uv;\n"
        "out vec2 game_position;\n"
        "out vec2 fragment_uv;\n"
        "void main() {\n"
        "  game_position = position.xy;\n"
        "  fragment_uv = uv;\n"
        "  gl_Position = projection * vec4(position.xy, 0, 1);\n"
        "}\n";
    compile_options.fragment_shader =
        "#version 330\n"
        "precision mediump float;\n"
        "uniform vec2 viewport_topleft = vec2(-65535, -65535);\n"
        "uniform vec2 viewport_bottomright = vec2(65535, 65535);\n"
        "uniform sampler2D tex;\n"
        "in vec2 game_position;\n"
        "in vec2 fragment_uv;\n"
        "out vec4 out_color;\n"
        "void main(){\n"
        "  vec2 inside = step(viewport_topleft, game_position) - step(viewport_bottomright, game_position);\n"
        "  float t = inside.x * inside.y;\n"
        "  out_color = t * texture(tex, fragment_uv.st);\n"
        "}\n";

    CHECK(gl::Program<Vertex>::compileandlink(
        &self->program,
        &compile_options),
        Error::UIERROR) << "failed to compile render program";

    LOG(Logger::INFO) << "shaders compiled";
    return Error();
}

}
