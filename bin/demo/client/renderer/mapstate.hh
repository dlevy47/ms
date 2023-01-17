#pragma once

#include <unordered_map>

#include "p.hh"
#include "client/dataset.hh"
#include "client/map.hh"
#include "client/universe.hh"
#include "client/game/renderer.hh"
#include "ms/game/mapstate.hh"
#include "util/error.hh"
#include "wz/vfs.hh"

namespace client {
namespace renderer {

struct MapLoader {
    P<client::Dataset> dataset;

    P<client::Universe> universe;

    client::Map::Helper map_helper;

    std::unordered_map<ms::Map::ID, client::Map, ms::Map::ID::Hash> maps;

    static Error init(
        MapLoader* loader,
        client::Dataset* dataset,
        client::Universe* universe);

    Error load(
        client::Map** map,
        ms::Map::ID id);

    MapLoader() = default;
    MapLoader(const MapLoader&) = delete;
    MapLoader(MapLoader&&) = default;
};

struct MapState {
    struct Options {
        struct {
            bool bounding_box{ false };
            bool footholds{ false };
            bool ladders{ false };
            bool portals{ false };

            float line_width{ 2 };
        } debug;

        bool background{ true };
        bool objects{ true };
        bool tiles{ true };
    };

    Error render(
        const Options* options,
        client::game::Renderer::Target* target,
        MapLoader* loader,
        const ms::game::MapState* state,
        uint64_t now) const;
};

}
}
