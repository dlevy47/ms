#pragma once

#include <variant>

#include "p.hh"
#include "systems/freelistvector.hh"

namespace systems {

struct Time {
    struct Component {
        struct Milliseconds {
            uint64_t milliseconds;
        };

        struct RealTime {
            float time;
        };

        struct Counter {
            // min is the (inclusive) minimum value of this counter.
            uint64_t min;
            // max is the (exclusive) maximum value of this counter.
            uint64_t max;

            // wrap specifies whether counting should wrap back around after max. If wrap
            // is false, counting will be clamped.
            bool wrap;

            // delay is the time to wait between ticks.
            uint64_t delay;

            // remaining is the number of milliseconds remaining in the current tick.
            uint64_t remaining;

            uint64_t value;
        };

        struct Animation {
            // frames is a list of delays to the next frame. I.e., frames[0] is the number
            // of milliseconds to wait before incrementing value.
            std::vector<uint64_t> frames;

            // remaining is the number of milliseconds remaining in the current frame.
            // A value of 0 signifies that that all milliseconds are remaining.
            uint64_t remaining;

            uint64_t value;
        };

        std::variant<
            std::monostate,
            Milliseconds,
            RealTime,
            Counter,
            Animation> value;
    };

    template <typename T>
    struct Handle {
        P<Time> system;
        N<size_t> index;

        T* operator->() {
            return &std::get<T>(
                    (*system->components.items[index]).value);
        }
        const T* operator->() const {
            return &std::get<T>(
                    (*system->components.items[index]).value);
        }

        Handle<T>& operator=(Handle<T>&& rhs) {
            system = std::exchange(rhs.system, nullptr);
            index = std::exchange(rhs.index, 0);

            return *this;
        }

        ~Handle() {
            if (system)
                system->components.remove(index);
        }

        Handle() = default;
        Handle(Handle<T>&&) = default;
        Handle(const Handle<T>&) = delete;
    };

    uint64_t last;

    FreelistVector<Component> components;

    template <typename T>
    Handle<T> add() {
        Component c;
        c.value.emplace<T>();
        size_t at = components.insert(std::move(c));

        Handle<T> ret;
        ret.system = this;
        ret.index = at;
        return ret;
    }

    void tick();

    static void init(
            Time* self,
            uint64_t now) {
        self->last = now;
    }

    Time() = default;
    // Move construction is deleted because this type hands out pointers to itself.
    Time(Time&&) = delete;
    Time(const Time&) = delete;
};

}
