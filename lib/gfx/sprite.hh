#pragma once

#include <vector>

#include "p.hh"
#include "gl.hh"
#include "gfx/vector.hh"
#include "gfx/vertex.hh"
#include "util/error.hh"
#include "wz/directory.hh"
#include "wz/vfs.hh"

namespace gfx {

struct Sprite {
    struct Frame {
        wz::Image image;
        N<GLuint> texture;

        Vector<int32_t> origin;
        int32_t delay;

        static Error load(
                Frame* self,
                wz::Image image,
                const uint8_t* image_data);

        static Error loadfromfile(
                Frame* self,
                const wz::OpenedFile::Node* node);

        // quad computes the 4 vertices of the quad suitable for drawing this frame
        // at the specified point. The vertex positions are computed so as to place
        // the origin of this frame at the provided point; that is, for an origin
        // that is within the frame (down and to the right), the vertex positions
        // are offset up and to the left.
        // Vertices are populated clockwise, starting at the top left.
        void quad(
                Vector<int32_t> at,
                gfx::Vertex vertices[4]) const;

        GLenum format() const {
            switch (image.format + image.format2) {
                case 1:
                case 2:
                    return GL_BGRA;
                case 513:
                case 517:
                    return GL_RGB;
            }

            // TODO: handle errors here?
            return 0;
        }

        GLenum type() const {
            switch (image.format + image.format2) {
                case 1:
                case 2:
                    return GL_UNSIGNED_BYTE;
                case 513:
                    return GL_UNSIGNED_SHORT_5_6_5;
                case 517:
                    return GL_UNSIGNED_SHORT_5_6_5_REV;
            }

            // TODO: handle errors here?
            return 0;
        }

        ~Frame() {
            if (texture)
                glDeleteTextures(1, &texture);
        }

        Frame() = default;
        Frame(Frame&& rhs) = default;
        Frame& operator=(Frame&& rhs) = default;
        Frame(const Frame&) = delete;
    };

    std::vector<Frame> frames;

    static Error loadfromfile(
            Sprite* self,
            const wz::OpenedFile::Node* node);
};

}
