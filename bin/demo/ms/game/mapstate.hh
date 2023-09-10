#pragma once

#include <atomic>
#include <variant>
#include <vector>

#include "p.hh"
#include "ms/map.hh"
#include "ms/mob.hh"
#include "util/error.hh"

namespace ms {
namespace game {

// MapInput represents a single update to be made to the state.
struct MapInput {
    // MobSpawn is an update to spawn a mob.
    struct MobSpawn {
        // id is the ID of the mob to spawn.
        ms::Mob::ID id;

        // instance_id is a unique ID to give the spawned mob that can be used
        // to reference the mob in future updates.
        uint64_t instance_id;
    };

    // MobDespawn is an update to despawn a mob, i.e. when killed.
    struct MobDespawn {
        // instance_id is the ID of the spawned mob to despawn.
        uint64_t instance_id;
    };

    // data is the content of this update.
    std::variant<
    MobSpawn,
    MobDespawn> data;

    // List is the atomic unit of state updates: a list of Updates is given to be
    // applied, in order, to the map.
    struct List {
        // base is the version of the MapState that this update list applies to.
        // This is used to ensure ordered serialization.
        uint64_t base;

        std::vector<MapInput> updates;
    };
};

struct MapOutput {
    // MobSpawn is an event that a mob spawned.
    struct MobSpawned {
        // id is the ID of the mob spawned.
        ms::Mob::ID id;

        // instance_id is the mob instance's unique id.
        uint64_t instance_id;
    };

    // MobDespawned is an update that a mob despawned.
    struct MobDespawned {
        // instance_id is the ID of the mob that despawned.
        uint64_t instance_id;
    };

    // data is the content of this update.
    std::variant<
    MobSpawned,
    MobDespawned> data;

    // List is an atomic unit of state events. It represents the changes
    // made to the state in one update.
    struct List {
        // base is the version of the MapState that produced these events.
        uint64_t base;

        std::vector<MapOutput> updates;
    };
};

struct MapState {
    // version is the version of this state.
    std::atomic<uint64_t> version;

    // basemap is the map that this room is based off of. Modifications can
    // be made to the map by various events/mobs/skills etc.
    ms::Map basemap;

    static Error init(
        MapState* state,
        ms::Map&& basemap);

    Error apply(
        MapOutput::List* outputs,
        const MapInput::List* inputs);
};

}
}
