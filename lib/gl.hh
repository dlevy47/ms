#pragma once

#include <string>
#include <unordered_map>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "p.hh"
#include "util/error.hh"

namespace gl {

Error init();

template <typename V>
struct Program {
    N<GLuint> program;

    std::unordered_map<std::string, GLuint> attributes;
    std::unordered_map<std::string, GLuint> uniforms;

    struct CompileOptions {
        const GLchar* vertex_shader;
        const GLchar* geometry_shader;
        const GLchar* fragment_shader;

        CompileOptions():
            vertex_shader(nullptr),
            geometry_shader(nullptr),
            fragment_shader(nullptr) {}
    };

    static Error compileandlink(
        Program* program,
        const CompileOptions* options);

    operator GLuint() const {
        return program;
    }

    Error attribute(
        const char* name,
        GLuint* a) const {
        auto it = attributes.find(name);
        if (it == attributes.end()) {
            return error_new(Error::GLERROR)
                << "no \"" << name << "\" attribute found in shaders";
        }

        *a = it->second;
        return Error();
    }

    ~Program() {
        if (program)
            glDeleteProgram(program);
    }

    Program() = default;
    Program(Program&&) = default;
    Program(const Program&) = delete;
};

template <typename V>
struct Drawable {
    N<GLuint> vao;
    N<GLuint> vbo;
    N<GLuint> ebo;

    GLenum vbo_mode;
    GLenum ebo_type;

    struct InitOptions {
        size_t vbo_init_size;
        size_t ebo_init_size;

        InitOptions():
            vbo_init_size(0),
            ebo_init_size(0) {}
    };

    static Error init(
        Drawable* self,
        const Program<V>* program,
        const InitOptions* options);

    void vbo_load(
        const V* vertices,
        size_t count) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            count * sizeof(V),
            vertices,
            GL_STREAM_DRAW);
    }

    template <typename E>
    void ebo_load_(
        const E* indices,
        size_t count) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            count * sizeof(E),
            indices,
            GL_STREAM_DRAW);
    }

    void ebo_load(
        const uint8_t* indices,
        size_t count) {
        ebo_type = GL_UNSIGNED_BYTE;
        ebo_load_(indices, count);
    }

    void ebo_load(
        const uint16_t* indices,
        size_t count) {
        ebo_type = GL_UNSIGNED_SHORT;
        ebo_load_(indices, count);
    }

    void ebo_load(
        const uint32_t* indices,
        size_t count) {
        ebo_type = GL_UNSIGNED_INT;
        ebo_load_(indices, count);
    }

    ~Drawable() {
        if (vao)
            glDeleteVertexArrays(1, &vao);

        if (vbo)
            glDeleteBuffers(1, &vbo);

        if (ebo)
            glDeleteBuffers(1, &ebo);
    }

    Drawable() = default;
    Drawable(Drawable&&) = default;
    Drawable(const Drawable&) = delete;
};

};
