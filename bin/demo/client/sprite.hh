#pragma once

#include "gfx/sprite.hh"
#include "gfx/vertex.hh"
#include "systems/time.hh"

namespace client {

struct Sprite {
    gfx::Sprite sprite;
    systems::Time::Handle<systems::Time::Component::Animation> time;

    static Error init(
        Sprite* self,
        gfx::Sprite&& sprite,
        systems::Time* time) {
        self->sprite = std::move(sprite);
        self->time = time->add<systems::Time::Component::Animation>();

        for (size_t i = 0, l = self->sprite.frames.size(); i < l; ++i) {
            self->time->frames.push_back(
                self->sprite.frames[i].delay);
        }

        return Error();
    }

    void quad(
        gfx::Vector<int32_t> at,
        gfx::Vertex vertices[4]) const {
        return frame()->quad(
            at,
            vertices);
    }

    const gfx::Sprite::Frame* frame() const {
        return &sprite.frames[time->value];
    }

    Sprite() = default;
    Sprite(const Sprite&) = delete;
    Sprite(Sprite&&) = default;
};

}
