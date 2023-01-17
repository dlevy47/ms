#pragma once

#include <unordered_map>
#include <variant>

#include "gl.hh"
#include "p.hh"
#include "gl/window.hh"
#include "gfx/program.hh"
#include "gfx/rect.hh"
#include "gfx/sprite.hh"
#include "gfx/linevertex.hh"
#include "util/error.hh"

namespace client {
namespace game {

// Renderer renders game-related things onto a window frame.
// Importantly, this Renderer only supports two drawing modes:
//   * Blitting textured quads onto the screen.
//   * Lines for debug purposes (footholds, collision boxes, etc).
// All inputs to the Renderer are expected to be in game coordinates. The
// Renderer will translate to screen coordinates.
struct Renderer {
    struct Target {
        P<Renderer> that;

        struct Metrics {
            size_t draw_calls{ 0 };
            size_t quads{ 0 };
            std::unordered_map<GLuint, std::monostate> seen_textures;

            size_t textures() const {
                return seen_textures.size();
            }
        };

        Metrics metrics;

        // target is the window frame to render to.
        // target is unowned because other things may render to the same frame
        // before or after this Renderer.
        P<gl::Window::Frame> target;

        // viewport is the rectangle, in game coordinates, that is being drawn.
        gfx::Rect<double> viewport;

        GLfloat projection[4][4];
        GLfloat inverse_projection[4][4];

        template <typename T>
        gfx::Vector<T> unproject(gfx::Vector<T> x) const {
            gfx::Vector<T> ret;
            ret.x =
                (x.x * inverse_projection[0][0]) +
                (x.y * inverse_projection[1][0]) +
                // z = 0
                inverse_projection[3][0];
            ret.y =
                (x.x * inverse_projection[0][1]) +
                (x.y * inverse_projection[1][1]) +
                // z = 0
                inverse_projection[3][1];

            return ret;
        }

        struct {
            std::vector<uint32_t> ebo;
            std::vector<gfx::LineVertex> vbo;
        } lines;

        // frame draws a textured quad, placed so that its origin is at the
        // specified location (in game coordinates).
        Error frame(
            const gfx::Sprite::Frame* frame,
            const gfx::Vector<int32_t> at);

        struct LineOptions {
            float width{ 2 };
            struct {
                uint8_t r{ 0xFF };
                uint8_t g{ 0x00 };
                uint8_t b{ 0xFF };
                uint8_t a{ 0xFF };
            } color;
        };

        inline void line(
            const gfx::Vector<int32_t> start,
            const gfx::Vector<int32_t> end) {
            line_withoptions(
                start,
                end,
                LineOptions());
        }

        void line_withoptions(
            const gfx::Vector<int32_t> start,
            const gfx::Vector<int32_t> end,
            const LineOptions options);

        inline void rect(
            const gfx::Rect<int32_t> r) {
            rect_withoptions(
                r,
                LineOptions());
        }

        void rect_withoptions(
            const gfx::Rect<int32_t> r,
            const LineOptions options);

        ~Target() {
            // TODO: If some kind of caching/queuing system is built, flush here.
            if (lines.ebo.size() && lines.vbo.size()) {
                that->line_drawable.ebo_load(
                    lines.ebo.data(),
                    lines.ebo.size());
                that->line_drawable.vbo_load(
                    lines.vbo.data(),
                    lines.vbo.size());

                glUseProgram(that->line_program.program);
                glBindVertexArray(that->line_drawable.vao);
                glBindBuffer(GL_ARRAY_BUFFER, that->line_drawable.vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, that->line_drawable.ebo);

                glDrawElements(
                    that->line_drawable.vbo_mode,
                    lines.ebo.size(),
                    that->line_drawable.ebo_type,
                    nullptr);
            }
        }

        Target() = default;
        Target(const Target&) = delete;
        Target(Target&&) = default;
    };

    gfx::Drawable drawable;

    gfx::Program program;

    gl::Drawable<gfx::LineVertex> line_drawable;

    gl::Program<gfx::LineVertex> line_program;

    static Error init(
        Renderer* that);

    // begin starts a new render session and returns a Target to render to.
    // viewport provides, in game coordinates, the part of the scene to show.
    Target begin(
        gl::Window::Frame* target,
        gfx::Rect<double> viewport);

    Renderer() = default;
    Renderer(Renderer&&) = default;
    Renderer(const Renderer&) = delete;
};

}
}
