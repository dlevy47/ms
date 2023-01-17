#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "gfx/rect.hh"
#include "gfx/vector.hh"
#include "util/error.hh"
#include "wz/vfs.hh"

namespace ms {

struct Map {
    struct ID {
        int32_t id;

        std::wstring padded() const;
        std::wstring as_filename() const;

        bool operator==(const ID& rhs) const {
            return id == rhs.id;
        }

        struct Hash {
            size_t operator()(const ID& id) const noexcept {
                return std::hash<int32_t>{}(id.id);
            }
        };

        static bool from(
            ID* self,
            const wchar_t* name);
    };

    struct Foothold {
        gfx::Vector<int32_t> start;
        gfx::Vector<int32_t> end;

        int32_t group;
        int32_t id;
        int32_t next_id;
        int32_t prev_id;
        double force;
        bool forbid_fall;

        Foothold():
            start(),
            end(),
            group(0),
            id(0),
            next_id(0),
            prev_id(0),
            force(false),
            forbid_fall(false) {}

        Foothold(Foothold&&) = default;
        Foothold(const Foothold&) = delete;
    };

    struct Ladder {
        gfx::Vector<int32_t> start;
        gfx::Vector<int32_t> end;

        bool forbid_climb;

        Ladder():
            start(),
            end(),
            forbid_climb() {}

        Ladder(Ladder&&) = default;
        Ladder(const Ladder&) = delete;
    };

    struct Portal {
        enum Kind {
            STARTPOINT,
            INVISIBLE,
            VISIBLE,
            COLLISION,
            CHANGEABLE,
            CHANGEABLEINVISIBLE,
            TOWN,
            SCRIPT,
            SCRIPTINVISIBLE,
            COLLISIONSCRIPT,
            HIDDEN,
            SCRIPTHIDDEN,
            COLLISIONJUMP,
            COLLISIONCUSTOM,
            COLLISIONCHANGEABLE,
        };

        Kind kind;

        const wchar_t* name;

        gfx::Vector<int32_t> at;

        ID target_map;

        const wchar_t* target_portal;

        Portal():
            kind(),
            name(nullptr),
            at(),
            target_map(),
            target_portal() {}

        Portal(Portal&&) = default;
        Portal(const Portal&) = delete;
    };

    struct Layer {
        std::vector<Foothold> footholds;
        std::vector<Ladder> ladders;

        Layer() = default;
        Layer(const Layer&) = delete;
    };

    ID id;

    wz::Vfs::File::Handle map_file;

    std::unordered_map<uint32_t, Layer> layers;

    std::vector<Portal> portals;

    gfx::Rect<int32_t> bounding_box;

    static Error load(
        Map* self,
        ID id,
        wz::Vfs::File::Handle&& map_file);

    Map() = default;
    Map(Map&&) = default;
    Map(const Map&) = delete;

    Map& operator=(Map&&) = default;
};

inline std::wostream& operator<<(std::wostream& os, ms::Map::ID id) {
    os << "[map] " << id.padded();
    return os;
}

}
