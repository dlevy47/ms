#include "client/game/renderer.hh"

#include "logger.hh"

namespace client {
namespace game {

Error Renderer::Target::frame(
    const gfx::Sprite::Frame* frame,
    const gfx::Vector<int32_t> at) {
    ++metrics.quads;
    metrics.seen_textures.insert(frame->texture);

    // TODO: Instead of drawing each frame separately with its own call,
    // submit to a draw chain.
    that->program.projection(projection);

    const uint8_t ebo[6] = { 0, 1, 3, 1, 2, 3 };
    that->drawable.ebo_load(
        ebo,
        sizeof(ebo) / sizeof(*ebo));

    gfx::Vertex quad[4] = { {0} };

    gfx::Vector<int32_t> topleft =
        at - frame->origin;
    gfx::Vector<int32_t> bottomright = {
        .x = topleft.x + static_cast<int32_t>(frame->image.width),
        .y = topleft.y + static_cast<int32_t>(frame->image.height),
    };

    gfx::Vector<float> topleft_uv = {
        .x = 0,
        .y = 0,
    };
    gfx::Vector<float> bottomright_uv = {
        .x = 1,
        .y = 1,
    };

    quad[0].position[0] =
        static_cast<float>(topleft.x);
    quad[0].position[1] =
        static_cast<float>(topleft.y);
    quad[0].uv[0] = topleft_uv.x;
    quad[0].uv[1] = topleft_uv.y;

    quad[1].position[0] =
        static_cast<float>(bottomright.x);
    quad[1].position[1] =
        static_cast<float>(topleft.y);
    quad[1].uv[0] = bottomright_uv.x;
    quad[1].uv[1] = topleft_uv.y;

    quad[2].position[0] =
        static_cast<float>(bottomright.x);
    quad[2].position[1] =
        static_cast<float>(bottomright.y);
    quad[2].uv[0] = bottomright_uv.x;
    quad[2].uv[1] = bottomright_uv.y;

    quad[3].position[0] =
        static_cast<float>(topleft.x);
    quad[3].position[1] =
        static_cast<float>(bottomright.y);
    quad[3].uv[0] = topleft_uv.x;
    quad[3].uv[1] = bottomright_uv.y;

    that->drawable.vbo_load(
        quad,
        sizeof(quad) / sizeof(*quad));

    glUseProgram(that->program.program);
    glBindVertexArray(that->drawable.vao);
    glBindBuffer(GL_ARRAY_BUFFER, that->drawable.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, that->drawable.ebo);

    glBindTexture(GL_TEXTURE_2D, frame->texture);

    glDrawElements(
        that->drawable.vbo_mode,
        6,
        that->drawable.ebo_type,
        nullptr);

    ++metrics.draw_calls;

    return Error();
}

void Renderer::Target::line_withoptions(
    const gfx::Vector<int32_t> start_i,
    const gfx::Vector<int32_t> end_i,
    const Renderer::Target::LineOptions options) {
    // Calculate thick lines to draw here:
    //   1. Calculate the vector represented by the line.
    //   2. Calculate a perpendicular vector.
    //   3. Scale the perpendicular vector so that it's length is line_width.
    //   4. Offset the line's endpoints by this vector, and it's reflection.
    glUseProgram(that->line_program.program);
    glUniformMatrix4fv(
        that->line_program.uniforms.at("projection"),
        1,
        GL_FALSE,
        &projection[0][0]);
    const gfx::Vector<float> start = (gfx::Vector<float>) start_i;
    const gfx::Vector<float> end = (gfx::Vector<float>) end_i;

    lines.ebo.push_back(lines.vbo.size());
    lines.ebo.push_back(lines.vbo.size() + 1);
    lines.ebo.push_back(lines.vbo.size() + 2);
    lines.ebo.push_back(lines.vbo.size() + 1);
    lines.ebo.push_back(lines.vbo.size() + 2);
    lines.ebo.push_back(lines.vbo.size() + 3);

    // 1.
    gfx::Vector<float> vector = end - start;

    // 2.
    gfx::Vector<float> perpendicular;
    perpendicular.x = -vector.y;
    perpendicular.y = vector.x;

    // 3.
    float length = ::sqrt((perpendicular.x * perpendicular.x) + (perpendicular.y * perpendicular.y));
    float scale = options.width / length;
    perpendicular *= scale;

    // 4.
    gfx::Vector<float> v1, v2, v3, v4;
    v1 = start + perpendicular;
    v2 = start - perpendicular;
    v3 = end + perpendicular;
    v4 = end - perpendicular;

    gfx::LineVertex v;
    v.color[0] = static_cast<float>(options.color.r); // / 0xFF;
    v.color[1] = static_cast<float>(options.color.g); // / 0xFF;
    v.color[2] = static_cast<float>(options.color.b); // / 0xFF;
    v.color[3] = static_cast<float>(options.color.a); // / 0xFF;

    v.position[0] = v1.x;
    v.position[1] = v1.y;
    lines.vbo.push_back(v);

    v.position[0] = v2.x;
    v.position[1] = v2.y;
    lines.vbo.push_back(v);

    v.position[0] = v3.x;
    v.position[1] = v3.y;
    lines.vbo.push_back(v);

    v.position[0] = v4.x;
    v.position[1] = v4.y;
    lines.vbo.push_back(v);
}

void Renderer::Target::rect_withoptions(
    const gfx::Rect<int32_t> r,
    const Target::LineOptions options) {
    const gfx::Vector<int32_t> topright = {
        .x = r.bottomright.x,
        .y = r.topleft.y,
    };
    const gfx::Vector<int32_t> bottomleft = {
        .x = r.topleft.x,
        .y = r.bottomright.y,
    };

    line_withoptions(
        r.topleft,
        topright,
        options);
    line_withoptions(
        topright,
        r.bottomright,
        options);
    line_withoptions(
        r.bottomright,
        bottomleft,
        options);
    line_withoptions(
        bottomleft,
        r.topleft,
        options);
}

Error Renderer::init(
    Renderer* that) {
    CHECK(gfx::Program::init(
        &that->program),
        Error::UIERROR) << "failed to load shader program";
    LOG(Logger::INFO) << "loaded render program";

    gfx::Drawable::InitOptions drawable_init_options;
    CHECK(gfx::Drawable::init(
        &that->drawable,
        &that->program.program,
        &drawable_init_options),
        Error::GLERROR) << "failed to init drawable";
    LOG(Logger::INFO) << "loaded render drawable";

    gl::Program<gfx::LineVertex>::CompileOptions compile_options;
    compile_options.vertex_shader =
        "#version 330\n"
        "uniform mat4 projection;\n"
        "in vec4 color;\n"
        "in vec2 position;\n"
        "out vec2 game_position;\n"
        "out vec4 fragment_color;\n"
        "void main() {\n"
        "  game_position = position.xy;\n"
        "  fragment_color = color;\n"
        "  gl_Position = projection * vec4(position.xy, 0, 1);\n"
        "}\n";
    compile_options.fragment_shader =
        "#version 330\n"
        "precision mediump float;\n"
        "uniform vec2 viewport_topleft = vec2(-65535, -65535);\n"
        "uniform vec2 viewport_bottomright = vec2(65535, 65535);\n"
        "in vec2 game_position;\n"
        "in vec4 fragment_color;\n"
        "out vec4 out_color;\n"
        "void main(){\n"
        "  vec2 inside = step(viewport_topleft, game_position) - step(viewport_bottomright, game_position);\n"
        "  float t = inside.x * inside.y;\n"
        "  out_color = t * fragment_color;\n"
        "}\n";
    CHECK(gl::Program<gfx::LineVertex>::compileandlink(
        &that->line_program,
        &compile_options),
        Error::UIERROR) << "failed to compile line program";
    LOG(Logger::INFO) << "loaded line program";

    gl::Drawable<gfx::LineVertex>::InitOptions line_drawable_init_options;
    CHECK(gl::Drawable<gfx::LineVertex>::init(
        &that->line_drawable,
        &that->line_program,
        &line_drawable_init_options),
        Error::GLERROR) << "failed to init line drawable";
    LOG(Logger::INFO) << "loaded line drawable";

    return Error();
}

static inline void matrixmultiply(
    GLfloat into[4][4],
    const GLfloat left[4][4],
    const GLfloat right[4][4]) {
#define MUL(x, y) \
    into[x][y] = \
        (left[0][y] * right[x][0]) + \
        (left[1][y] * right[x][1]) + \
        (left[2][y] * right[x][2]) + \
        (left[3][y] * right[x][3])
    MUL(0, 0);
    MUL(1, 0);
    MUL(2, 0);
    MUL(3, 0);
    MUL(0, 1);
    MUL(1, 1);
    MUL(2, 1);
    MUL(3, 1);
    MUL(0, 2);
    MUL(1, 2);
    MUL(2, 2);
    MUL(3, 2);
    MUL(0, 3);
    MUL(1, 3);
    MUL(2, 3);
    MUL(3, 3);
#undef MUL
}

static void printmatrix(GLfloat m[4][4]) {
    std::cout << "[";
    std::cout << m[0][0] << ", " << m[1][0] << ", " << m[2][0] << ", " << m[3][0] << "; ";
    std::cout << m[0][1] << ", " << m[1][1] << ", " << m[2][1] << ", " << m[3][1] << "; ";
    std::cout << m[0][2] << ", " << m[1][2] << ", " << m[2][2] << ", " << m[3][2] << "; ";
    std::cout << m[0][3] << ", " << m[1][3] << ", " << m[2][3] << ", " << m[3][3];
    std::cout << "]";
}

Renderer::Target Renderer::begin(
    gl::Window::Frame* target,
    gfx::Rect<uint32_t> window_viewport,
    gfx::Rect<double> game_viewport) {
    Renderer::Target render_target;
    render_target.that = this;
    render_target.target = target;
    render_target.game_viewport = game_viewport;
    
    if (glIsEnabled(GL_SCISSOR_TEST)) {
        GLint box[4] = {0};

        glGetIntegerv(
            GL_SCISSOR_BOX,
            box);

        render_target.previous_scissor = gfx::Rect<GLint>{
            .topleft = {box[0], box[1]},
            .bottomright = {box[0] + box[2], box[1] + box[3]},
        };
    }
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        (GLint) window_viewport.topleft.x,
        (GLint) window_viewport.topleft.y,
        (GLint) window_viewport.width(),
        (GLint) window_viewport.height());

    // Convert window_viewport to ndc_viewport.
    gfx::Rect<GLfloat> ndc_viewport = {
        .topleft = (gfx::Vector<GLfloat>) window_viewport.topleft - target->viewport.topleft,
        .bottomright = (gfx::Vector<GLfloat>) window_viewport.bottomright - target->viewport.topleft,
    };
    ndc_viewport.topleft.x =
        ((ndc_viewport.topleft.x / target->viewport.width()) * 2) - 1;
    ndc_viewport.topleft.y =
        ((ndc_viewport.topleft.y / target->viewport.height()) * 2) - 1;
    ndc_viewport.bottomright.x =
        ((ndc_viewport.bottomright.x / target->viewport.width()) * 2) - 1;
    ndc_viewport.bottomright.y =
        ((ndc_viewport.bottomright.y / target->viewport.height()) * 2) - 1;

    // The projection matrix is a multiplication of:
    //   T * S * P
    // where:
    //   * P is the orthographic projection from game_viewport to NDC
    //   * S is a scale matrix to scale game_viewport to the size of ndc_viewport
    //   * T is a translation matrix to translate the projection to ndc_viewport
    // To make the code clearer, we compute the terms of the final matrix using the
    // following notation:
    //
    //   m1 0  0  m3
    //   0  m2 0  m4
    //   0  0  1  0
    //   0  0  0  1
    
    gfx::Vector<GLfloat> ortho_translate =
        (gfx::Vector<GLfloat>) game_viewport.topleft * -1;
    gfx::Vector<GLfloat> ortho_scale =
        (gfx::Vector<GLfloat>) (game_viewport.bottomright - game_viewport.topleft);

    // The terms of the projection matrix.
    const GLfloat p1 = 2 / ortho_scale.x;
    const GLfloat p2 = -2 / ortho_scale.y;
    const GLfloat p3 = ((2 / ortho_scale.x) * ortho_translate.x) - 1;
    const GLfloat p4 = 1 - ((2 / ortho_scale.y) * ortho_translate.y);

    gfx::Vector<GLfloat> window_scale =
        (gfx::Vector<GLfloat>) (ndc_viewport.bottomright - ndc_viewport.topleft) / 2;
    gfx::Vector<GLfloat> window_translate =
        (gfx::Vector<GLfloat>) (ndc_viewport.topleft - gfx::Vector<double>{-1.0, -1.0});
    window_translate.y *= -1;  // why?

    // The terms of the final matrix.
    const GLfloat m1 = window_scale.x * p1;
    const GLfloat m2 = window_scale.y * p2;
    const GLfloat m3 = (window_scale.x * p3) + window_translate.x;
    const GLfloat m4 = (window_scale.y * p4) + window_translate.y;

    memset(render_target.projection, 0, sizeof(render_target.projection));

    render_target.projection[0][0] = m1;

    render_target.projection[1][1] = m2;

    render_target.projection[2][2] = 1;

    render_target.projection[3][0] = m3;
    render_target.projection[3][1] = m4;
    render_target.projection[3][3] = 1;

    memset(render_target.inverse_projection, 0, sizeof(render_target.inverse_projection));

    render_target.inverse_projection[0][0] = 1 / m1;

    render_target.inverse_projection[1][1] = 1 / m2;

    render_target.inverse_projection[2][2] = 1;

    render_target.inverse_projection[3][0] = -m3 / m1;
    render_target.inverse_projection[3][1] = -m4 / m2;
    render_target.inverse_projection[3][3] = 1;

    return render_target;
}

}
}
