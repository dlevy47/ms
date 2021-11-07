#include "systems/time.hh"

#include "client/time.hh"

namespace systems {

void Time::tick() {
    const auto now = client::now();
    const uint64_t now_ms = client::now().to_milliseconds();
    const uint64_t interval = now_ms - last;

    for (size_t i = 0, l = components.items.size(); i < l; ++i) {
        if (!components.items[i].has_value())
            continue;

        Component& component = *components.items[i];
        if (std::holds_alternative<Component::RealTime>(component.value)) {
            Component::RealTime& x = std::get<Component::RealTime>(component.value);

            x.time = now.to_float();
        } else if (std::holds_alternative<Component::Milliseconds>(component.value)) {
            Component::Milliseconds& x = std::get<Component::Milliseconds>(component.value);

            x.milliseconds += interval;
        } else if (std::holds_alternative<Component::Counter>(component.value)) {
            Component::Counter& x = std::get<Component::Counter>(component.value);

            if (x.remaining == 0)
                x.remaining = x.delay;

            uint64_t increment = interval / x.remaining;
            x.value += increment;
            if (x.value >= x.max) {
                if (x.wrap)
                    x.value = x.min;
                else
                    x.value = x.max - 1;
            }

            x.remaining = interval % x.remaining;
        } else if (std::holds_alternative<Component::Animation>(component.value)) {
            Component::Animation& x = std::get<Component::Animation>(component.value);

            // Special case: don't update any animations with just one frame.
            if (x.frames.size() <= 1)
                continue;

            if (x.remaining == 0)
                x.remaining = x.frames[x.value];

            uint64_t this_interval = interval;
            while (this_interval >= x.remaining) {
                this_interval -= x.remaining;

                ++x.value;
                if (x.value >= x.frames.size())
                    x.value = 0;
                x.remaining = x.frames[x.value];
            }

            x.remaining -= this_interval;
        }
    }

    last = now_ms;
}

}
