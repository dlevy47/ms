#include "nk/ui.hh"

#include "logger.hh"
#include "ui.hh"

namespace nk {

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

    GLuint color = 0;
    CHECK(program->attribute(
                "color",
                &color),
            Error::GLERROR)
        << "failed to configure program";

    glEnableVertexAttribArray(position);
    glEnableVertexAttribArray(uv);
    glEnableVertexAttribArray(color);

    GLsizei stride = sizeof(Vertex);
    size_t position_offset = offsetof(Vertex, position);
    size_t uv_offset = offsetof(Vertex, uv);
    size_t color_offset = offsetof(Vertex, color);

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
    glVertexAttribPointer(
            color,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,
            stride,
            reinterpret_cast<void*>(color_offset));

    return Error();
}

Error Program::init(
        Program* self) {
    LOG(Logger::INFO) << "compiling shaders";
    gl::Program<Vertex>::CompileOptions compile_options;
    compile_options.vertex_shader = 
        "#version 330\n"
        "uniform mat4 projection;\n"
        "in vec2 position;\n"
        "in vec2 uv;\n"
        "in vec4 color;\n"
        "out vec2 fragment_uv;\n"
        "out vec4 fragment_color;\n"
        "void main() {\n"
        "  fragment_uv = uv;\n"
        "  fragment_color = color;\n"
        "  gl_Position = projection * vec4(position.xy, 0, 1);\n"
        "}\n";
    compile_options.fragment_shader =
        "#version 330\n"
        "precision mediump float;\n"
        "uniform sampler2D tex;\n"
        "in vec2 fragment_uv;\n"
        "in vec4 fragment_color;\n"
        "out vec4 out_color;\n"
        "void main(){\n"
        "  vec4 texel = texture(tex, fragment_uv.st);\n"
        "  out_color = fragment_color * texel;\n"
        "}\n";

    CHECK(gl::Program<Vertex>::compileandlink(
                &self->program,
                &compile_options),
            Error::UIERROR) << "failed to compile render program";

    LOG(Logger::INFO) << "shaders compiled";
    return Error();
}

void Ui::input(
        gl::Window* window,
        bool keys) {
    nk_input_begin(context); {
        double this_mouse_position[2] = {0};
        glfwGetCursorPos(
                window->window,
                &this_mouse_position[0],
                &this_mouse_position[1]);

        nk_input_motion(
                context,
                (int) this_mouse_position[0],
                (int) this_mouse_position[1]);
        nk_input_button(
                context,
                NK_BUTTON_LEFT,
                (int) this_mouse_position[0],
                (int) this_mouse_position[1],
                glfwGetMouseButton(window->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

        gfx::Vector<double> window_scroll;
        if (window->consume_scroll(&window_scroll)) {
            struct nk_vec2 scroll = {
                .x = static_cast<float>(window_scroll.x),
                .y = static_cast<float>(window_scroll.y),
            };
            nk_input_scroll(context, scroll);
        }

        if (keys) {
            while (!window->key_codepoints.empty()) {
                nk_input_unicode(context, window->key_codepoints.front());
                window->key_codepoints.pop();
            }

            while (!window->keys.empty()) {
                auto keypress = window->keys.front();

                enum nk_keys nk_key = NK_KEY_NONE;
                switch (keypress.key) {
                    case GLFW_KEY_BACKSPACE:
                        nk_key = NK_KEY_BACKSPACE;
                        break;
                    default:
                        break;
                }

                if (nk_key != NK_KEY_NONE) {
                    nk_input_key(
                            context,
                            nk_key,
                            keypress.action == GLFW_RELEASE ? 0 : 1);
                }

                window->keys.pop();
            }

            nk_input_key(
                    context,
                    NK_KEY_BACKSPACE,
                    glfwGetKey(window->window, GLFW_KEY_BACKSPACE) == GLFW_RELEASE ? 0 : 1);
        }
    }
    nk_input_end(context);
}

static Error Ui_init_fonts(
        Ui* self,
        nk_font** font_out) {
    LOG(Logger::INFO)
        << "loading fonts";

    GLuint font_texture = 0;
    nk_font* font = nullptr;

    struct nk_font_atlas atlas_ = {0};
    nk_font_atlas_init(&atlas_, &nk::Allocator);
    self->atlas.set(atlas_);
    {
        glGenTextures(1, &font_texture);

        int img_width = 0;
        int img_height = 0;
        nk_font_atlas_begin(self->atlas);
        font = nk_font_atlas_add_default(self->atlas, 13, nullptr);

        const void* img = nk_font_atlas_bake(
                self->atlas,
                &img_width,
                &img_height,
                NK_FONT_ATLAS_RGBA32);

        glBindTexture(GL_TEXTURE_2D, font_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA,
                (GLsizei) img_width, (GLsizei) img_height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, img);

        nk_font_atlas_end(
                self->atlas,
                nk_handle_id((int) font_texture),
                &self->null_texture);
    }
    LOG(Logger::INFO)
        << "fonts loaded";

    *font_out = font;
    return Error();
}

Error Ui::init(
        Ui* self) {
    nk_font* font = nullptr;
    CHECK(Ui_init_fonts(self, &font),
            Error::UIERROR) << "failed to initialize fonts";

    struct nk_context context_ = {0};
    if (!nk_init(&context_, &nk::Allocator, &font->handle)) {
        return error_new(Error::UIERROR)
            << "failed to initialize nk context";
    }
    self->context.set(context_);

    {
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(Vertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(Vertex, uv)},
            {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(Vertex, color)},
            {NK_VERTEX_LAYOUT_END}
        };
        self->convert_config.shape_AA = NK_ANTI_ALIASING_ON;
        self->convert_config.line_AA = NK_ANTI_ALIASING_ON;
        self->convert_config.vertex_layout = vertex_layout;
        self->convert_config.vertex_size = sizeof(Vertex);
        self->convert_config.vertex_alignment = NK_ALIGNOF(Vertex);
        self->convert_config.circle_segment_count = 22;
        self->convert_config.curve_segment_count = 22;
        self->convert_config.arc_segment_count = 22;
        self->convert_config.global_alpha = 1.0f;
        self->convert_config.null = self->null_texture;
    }

    return Error();
}

}

#include "gl.impl.cc"

template struct gl::Program<nk::Vertex>;
template struct gl::Drawable<nk::Vertex>;
