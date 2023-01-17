#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "gl.hh"
#include "p.hh"
#include "client/dataset.hh"
#include "client/sprite.hh"
#include "client/time.hh"
#include "client/universe.hh"
#include "ms/map.hh"
#include "gfx/program.hh"
#include "gfx/sprite.hh"
#include "gfx/vector.hh"
#include "util/error.hh"
#include "wz/vfs.hh"

namespace client {

struct Map {
    struct Helper {
        wz::Vfs::File::Handle map_helper_file;

        std::unordered_map<ms::Map::Portal::Kind, gfx::Sprite> portal_sprites;

        static Error load(
            Helper* self,
            wz::Vfs::File::Handle&& map_helper_file);

        Helper() = default;
        Helper(const Helper&) = delete;
        Helper(Helper&&) = default;
    };

    struct TileSet {
        struct Tile {
            struct Name {
                const wchar_t* u;
                int32_t no;

                struct Hash {
                    size_t operator()(const Name& n) const noexcept {
                        size_t h1 = std::hash<std::wstring_view>{}(std::wstring_view(n.u));
                        size_t h2 = std::hash<int32_t>{}(n.no);
                        return h1 ^ (h2 << 1);
                    }
                };

                bool operator==(const Name& rhs) const {
                    return ::wcscmp(u, rhs.u) == 0 && no == rhs.no;
                }
            };

            int32_t z;

            gfx::Sprite::Frame frame;

            Tile() = default;
            Tile(Tile&&) = default;

            Tile(const Tile&) = delete;
        };

        // tileset_file is a handle to the Vfs::File underlying this tile set.
        wz::Vfs::File::Handle tileset_file;

        std::unordered_map<Tile::Name, Tile, Tile::Name::Hash> tiles;

        TileSet() = default;
        TileSet(TileSet&&) = default;
        TileSet(const TileSet&) = delete;
    };

    // tilesets is a map from tileset name (tileset file basename) to TileSet.
    std::unordered_map<std::wstring, TileSet> tilesets;

    struct ObjectSet {
        struct Object {
            struct Name {
                const wchar_t* l0;
                const wchar_t* l1;
                const wchar_t* l2;

                struct Hash {
                    size_t operator()(const Name& n) const noexcept {
                        size_t h1 = std::hash<std::wstring_view>{}(std::wstring_view(n.l0));
                        size_t h2 = std::hash<std::wstring_view>{}(std::wstring_view(n.l1));
                        size_t h3 = std::hash<std::wstring_view>{}(std::wstring_view(n.l2));
                        return h1 ^ (h2 << 1) ^ (h3 << 2);
                    }
                };

                bool operator==(const Name& rhs) const {
                    return (::wcscmp(l0, rhs.l0) == 0 &&
                        ::wcscmp(l1, rhs.l1) == 0 &&
                        ::wcscmp(l2, rhs.l2) == 0);
                }
            };

            client::Sprite sprite;

            Object() = default;
            Object(Object&&) = default;

            Object(const Object&) = delete;
        };

        // objectset_file is a handle to the Vfs::File underlying this tile set.
        wz::Vfs::File::Handle objectset_file;

        std::unordered_map<Object::Name, Object, Object::Name::Hash> objects;

        ObjectSet() = default;
        ObjectSet(ObjectSet&&) = default;
        ObjectSet(const ObjectSet&) = delete;
    };

    // objectsets is a map from objectset name (objectset file basename) to ObjectSet.
    std::unordered_map<std::wstring, ObjectSet> objectsets;

    struct Layer {
        N<uint32_t> index;
        P<const TileSet> tileset;

        struct Tile {
            const Map::TileSet::Tile* tile;
            gfx::Vector<int32_t> position;
            int32_t z;
        };

        std::vector<Tile> tiles;

        struct Object {
            const Map::ObjectSet::Object* object;
            gfx::Vector<int32_t> position;
            int32_t z;
            int32_t f;
        };

        std::vector<Object> objects;

        Layer() = default;
        Layer(Layer&&) = default;
        Layer(const Layer&) = delete;
    };

    std::vector<Layer> layers;

    struct Background {
        enum Kind {
            SINGLE,
            TILEDHORIZONTAL,
            TILEDVERTICAL,
            TILEDBOTH,
            TILEDHORIZONTALMOVINGHORIZONTAL,
            TILEDVERTICALMOVINGVERTICAL,
            TILEDBOTHMOVINGHORIZONTAL,
            TILEDBOTHMOVINGVERTICAL,
            KIND_COUNT,
        };

        // background_file is a handle to the Vfs::File underlying this background.
        wz::Vfs::File::Handle background_file;

        gfx::Vector<int32_t> position;
        gfx::Vector<int32_t> c;
        gfx::Vector<int32_t> r;
        int32_t z;
        int32_t f;
        int32_t ani;
        // TODO: Instead of using an enum of kind here, how about a set of flags?
        Kind kind;
        int32_t front;

        gfx::Sprite::Frame frame;

        Background() = default;
        Background(Background&&) = default;
        Background& operator=(Background&&) = default;
        Background(const Background&) = delete;
    };

    // map_file is a handle to the Vfs::File underlying this map.
    wz::Vfs::File::Handle map_file;

    // backgrounds is the list of Backgrounds of this map. Backgrounds
    // that could not be loaded are skipped.
    std::vector<Background> backgrounds;

    // bounding_box is the rectangle, in game coordinates, that bounds this
    // map.
    gfx::Rect<int32_t> bounding_box;

    // time is a handle with the current time.
    systems::Time::Handle<systems::Time::Component::Milliseconds> time;

    struct LoadResults {
        std::map<std::wstring, std::wstring> backgrounds_missing;
        std::map<std::wstring, std::wstring> tilesets_missing;
        std::map<std::wstring, std::wstring> tiles_missing;
        std::map<std::wstring, std::wstring> objectsets_missing;
        std::map<std::wstring, std::wstring> objects_missing;

        bool empty() const {
            return
                backgrounds_missing.size() == 0 &&
                tilesets_missing.size() == 0 &&
                tiles_missing.size() == 0 &&
                objectsets_missing.size() == 0 &&
                objects_missing.size() == 0;
        }
    };

    // load loads Map information from an opened WZ file. Non-catastrophic errors,
    // such as missing backgrounds or missing tiles are not returned as Errors (i.e.
    // the load will still continue). Instead, they are reported in the LoadResults,
    // if a pointer to one has been provided.
    static Error load(
        client::Universe* universe,
        Map* self,
        wz::Vfs* map_vfs,
        wz::Vfs::File::Handle&& map_file,
        LoadResults* results);

    Map() = default;
    Map(const Map&) = delete;
    Map(Map&&) = default;
};

}
