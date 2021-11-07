#include "gl.hh"

namespace gl {

static std::string Program_compileandlink_getshadererror(
        GLuint shader) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    std::string message(length, '\0');
    glGetShaderInfoLog(shader, length, &length, message.data());

    return message;
}

template <typename V>
Error Program<V>::compileandlink(
        Program* program,
        const Program::CompileOptions* options) {
    GLint status;

    // TODO: Properly clean up on failure here.
    program->program = glCreateProgram();

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertex_shader, 1, &options->vertex_shader, 0);
    glShaderSource(fragment_shader, 1, &options->fragment_shader, 0);
    glCompileShader(vertex_shader);
    glCompileShader(fragment_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        return error_new(Error::GLERROR)
            << "failed to compile vertex shader: "
            << Program_compileandlink_getshadererror(vertex_shader).c_str();
    }

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        return error_new(Error::GLERROR)
            << "failed to compile fragment shader: "
            << Program_compileandlink_getshadererror(fragment_shader).c_str();
    }

    glAttachShader(*program, vertex_shader);

    if (options->geometry_shader != nullptr) {
        GLuint geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);

        glShaderSource(geometry_shader, 1, &options->geometry_shader, 0);
        glCompileShader(geometry_shader);

        glGetShaderiv(geometry_shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            return error_new(Error::GLERROR)
                << "failed to compile geometry shader: "
                << Program_compileandlink_getshadererror(geometry_shader).c_str();
        }

        glAttachShader(*program, geometry_shader);
    }

    glAttachShader(*program, fragment_shader);
    glLinkProgram(*program);

    glGetProgramiv(*program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &length);

        std::string message(length, '\0');
        glGetProgramInfoLog(*program, length, &length, message.data());

        return error_new(Error::GLERROR)
            << "failed to link program: " << message.c_str();
    }

    GLint attribute_count = 0;
    glGetProgramiv(*program, GL_ACTIVE_ATTRIBUTES, &attribute_count);

    GLint attribute_name_length = 0;
    glGetProgramiv(*program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attribute_name_length);

    std::string attribute_name(attribute_name_length, '\0');
    for (GLint i = 0; i < attribute_count; ++i) {
        GLint size = 0;
        GLenum type;

        glGetActiveAttrib(
                *program,
                static_cast<GLuint>(i),
                attribute_name_length,
                nullptr, 
                &size,
                &type,
                attribute_name.data());

        program->attributes[attribute_name.c_str()] = glGetAttribLocation(*program, attribute_name.c_str());
    }

    GLint uniform_count = 0;
    glGetProgramiv(*program, GL_ACTIVE_UNIFORMS, &uniform_count);

    GLint uniform_name_length = 0;
    glGetProgramiv(*program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_name_length);

    std::string uniform_name(uniform_name_length, '\0');
    for (GLint i = 0; i < uniform_count; ++i) {
        GLint size = 0;
        GLenum type;

        glGetActiveUniform(
                *program,
                static_cast<GLuint>(i),
                uniform_name_length,
                nullptr, 
                &size,
                &type,
                uniform_name.data());

        program->uniforms[uniform_name.c_str()] = glGetUniformLocation(*program, uniform_name.c_str());
    }

    return Error();
}

template <typename V>
Error Drawable<V>::init(
        Drawable* self,
        const Program<V>* program,
        const InitOptions* options) {
    self->vbo_mode = GL_TRIANGLES;
    glGenVertexArrays(1, &self->vao);
    glGenBuffers(1, &self->vbo);
    glGenBuffers(1, &self->ebo);

    CHECK(V::configure(
                program,
                self),
            Error::GLERROR)
        << "failed to configure VAO for vertex";

    if (options->vbo_init_size) {
        glBindBuffer(GL_ARRAY_BUFFER, self->vbo);
        glBufferData(
                GL_ARRAY_BUFFER,
                options->vbo_init_size,
                nullptr,
                GL_DYNAMIC_DRAW);
    }

    if (options->ebo_init_size) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self->ebo);
        glBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                options->ebo_init_size,
                nullptr,
                GL_DYNAMIC_DRAW);
    }

    return Error();
}

}
