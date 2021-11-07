#pragma once

#include "systems/time.hh"

namespace client {

struct Universe {
    systems::Time time;

    void tick() {
        time.tick();
    }
};

}
