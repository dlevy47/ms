#include "ms/game/mapstate.hh"

namespace ms {
namespace game {

Error MapState::init(
    MapState* state,
    ms::Map&& basemap) {
    state->version = 0;
    state->basemap = std::move(basemap);

    return Error();
}

Error MapState::apply(
    const MapState::Update::List* updates) {
    // TODO: handle updates.
    return Error();
}

}
}
