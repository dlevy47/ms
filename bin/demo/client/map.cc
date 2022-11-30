#include "client/map.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include <string>

#include "logger.hh"
#include "gfx/vertex.hh"
#include "util/convert.hh"
#include "wz/property.hh"

namespace client {

static Error Map_Helper_load_portalkind(
        ms::Map::Portal::Kind* kind,
        const wchar_t* name) {
    if (::wcscmp(name, L"sp") == 0) {
        *kind = ms::Map::Portal::STARTPOINT;
    } else if (::wcscmp(name, L"pi") == 0) {
        *kind = ms::Map::Portal::INVISIBLE;
    } else if (::wcscmp(name, L"pv") == 0) {
        *kind = ms::Map::Portal::VISIBLE;
    } else if (::wcscmp(name, L"pc") == 0) {
        *kind = ms::Map::Portal::COLLISION;
    } else if (::wcscmp(name, L"pg") == 0) {
        *kind = ms::Map::Portal::CHANGEABLE;
    } else if (::wcscmp(name, L"pgi") == 0) {
        *kind = ms::Map::Portal::CHANGEABLEINVISIBLE;
    } else if (::wcscmp(name, L"tp") == 0) {
        *kind = ms::Map::Portal::TOWN;
    } else if (::wcscmp(name, L"ps") == 0) {
        *kind = ms::Map::Portal::SCRIPT;
    } else if (::wcscmp(name, L"psi") == 0) {
        *kind = ms::Map::Portal::SCRIPTINVISIBLE;
    } else if (::wcscmp(name, L"pcs") == 0) {
        *kind = ms::Map::Portal::COLLISIONSCRIPT;
    } else if (::wcscmp(name, L"ph") == 0) {
        *kind = ms::Map::Portal::HIDDEN;
    } else if (::wcscmp(name, L"psh") == 0) {
        *kind = ms::Map::Portal::SCRIPTHIDDEN;
    } else if (::wcscmp(name, L"pcj") == 0) {
        *kind = ms::Map::Portal::COLLISIONJUMP;
    } else if (::wcscmp(name, L"pci") == 0) {
        *kind = ms::Map::Portal::COLLISIONCUSTOM;
    } else if (::wcscmp(name, L"pcig") == 0) {
        *kind = ms::Map::Portal::COLLISIONCHANGEABLE;
    } else {
        return error_new(Error::MAP_LOAD_HELPERLOADFAILED)
            << "unexpected portal kind name " << name;
    }

    return Error();
} 

Error Map::Helper::load(
        Map::Helper* self,
        wz::Vfs::File::Handle&& map_helper_file) {
    self->map_helper_file = std::move(map_helper_file);

    const wz::OpenedFile::Node* portals_node =
        self->map_helper_file->find(L"portal");
    if (portals_node == nullptr) {
        return error_new(Error::MAP_LOAD_HELPERLOADFAILED)
            << "no portal node";
    }

    const wz::OpenedFile::Node* editor_node =
        portals_node->find(L"editor");
    if (editor_node == nullptr) {
        return error_new(Error::MAP_LOAD_HELPERLOADFAILED)
            << "no editor node";
    }

    auto editors_it = editor_node->iterator();

    const wz::OpenedFile::Node* portal = nullptr;
    while ((portal = editors_it.next())) {
        ms::Map::Portal::Kind kind;
        CHECK(Map_Helper_load_portalkind(
                    &kind,
                    portal->name),
                Error::MAP_LOAD_HELPERLOADFAILED)
            << "failed to determine editor portal kind";

        gfx::Sprite sprite;

        gfx::Sprite::Frame frame;
        CHECK(gfx::Sprite::Frame::loadfromfile(
                    &frame,
                    portal),
                Error::MAP_LOAD_HELPERLOADFAILED)
            << "failed to load editor portal frame";

        sprite.frames.emplace_back(std::move(frame));

        self->portal_sprites.emplace(kind, std::move(sprite));
    }

    auto game_node =
        portals_node->find(L"game");
    if (game_node == nullptr) {
        return error_new(Error::MAP_LOAD_HELPERLOADFAILED)
            << "no game node";
    }

    // pv is a regular sprite.
    {
        const wz::OpenedFile::Node* pv_node =
            game_node->find(L"pv");
        if (pv_node == nullptr) {
            return error_new(Error::MAP_LOAD_HELPERLOADFAILED)
                << "no pv node";
        }

        gfx::Sprite pv_sprite;
        CHECK(gfx::Sprite::loadfromfile(
                    &pv_sprite,
                    pv_node),
                Error::MAP_LOAD_HELPERLOADFAILED)
            << "failed to load pv sprite";

        self->portal_sprites.emplace(
                ms::Map::Portal::VISIBLE,
                std::move(pv_sprite));
    }

    return Error();
}

static Error TileSet_load(
        wz::Vfs* map_vfs,
        Map::TileSet* tileset,
        const wchar_t* tileset_name_s,
        Map::LoadResults* results) {
    // Open the tileset file.
    wz::Vfs::Node* tileset_file_node = nullptr;
    std::wstring tileset_file_node_name;
    {
        std::wstringstream ss;
        ss << "Tile/" << tileset_name_s << ".img";
        tileset_file_node_name = ss.str();

        tileset_file_node = map_vfs->find(tileset_file_node_name.c_str());
    }
    if (tileset_file_node == nullptr || tileset_file_node->file() == nullptr) {
        return error_new(Error::TILESET_LOAD_MISSINGFILE)
            << "missing or invalid tileset file: " << tileset_file_node_name;
    }

    CHECK(tileset_file_node->file()->open(&tileset->tileset_file),
            Error::OPENFAILED) << "failed to open tileset file";

    auto group_it = tileset->tileset_file->iterator();

    const wz::OpenedFile::Node* group = nullptr;
    while ((group = group_it.next())) {
        if (std::wcscmp(group->name, L"info") == 0)
            continue;

        const wchar_t* u = group->name;
        auto number_it = group->iterator();

        const wz::OpenedFile::Node* number = nullptr;
        while ((number = number_it.next())) {
            if (number->kind != wz::Property::CANVAS) {
                if (results) {
                    std::wstringstream key_ss;
                    key_ss << tileset_name_s << "/" << u << "/" << number->name;

                    results->tiles_missing[key_ss.str()] = L"property is not an image";
                }
                continue;
            }

            int32_t no = 0;
            {
                if (util::convert(number->name, &no)) {
                    if (results) {
                        std::wstringstream key_ss;
                        key_ss << tileset_name_s << "/" << u << "/" << number->name;

                        std::wstringstream value_ss;
                        value_ss << "node name is not an int32_t";
                        results->tiles_missing[key_ss.str()] = value_ss.str();
                    }
                    continue;
                }
            }

            Map::TileSet::Tile tile;

            // Load the frame.
            CHECK(gfx::Sprite::Frame::load(
                        &tile.frame,
                        number->canvas.image,
                        number->canvas.image_data),
                    Error::TILESET_LOAD_FRAMELOADFAILED) << "failed to load tile frame";

            CHECK(number->childvector(
                        L"origin",
                        &tile.frame.origin.x,
                        &tile.frame.origin.y),
                    Error::BACKGROUND_LOAD_MISSINGATTRIBUTES) << "failed to load frame origin";

            CHECK(number->childint32(L"z", &tile.z),
                    Error::TILE_LOAD_MISSINGATTRIBUTES) << "missing or invalid z";

            const Map::TileSet::Tile::Name name = {
                .u = u,
                .no = no,
            };
            tileset->tiles.emplace(name, std::move(tile));
        }
    }

    return Error();
}

static Error ObjectSet_load(
        client::Universe* universe,
        wz::Vfs* map_vfs,
        Map::ObjectSet* objectset,
        const wchar_t* objectset_name_s,
        Map::LoadResults* results) {
    // Open the objectset file.
    wz::Vfs::Node* objectset_file_node = nullptr;
    std::wstring objectset_file_node_name;
    {
        std::wstringstream ss;
        ss << "Obj/" << objectset_name_s << ".img";
        objectset_file_node_name = ss.str();

        objectset_file_node = map_vfs->find(objectset_file_node_name.c_str());
    }
    if (objectset_file_node == nullptr || objectset_file_node->file() == nullptr) {
        return error_new(Error::OBJECTSET_LOAD_MISSINGFILE)
            << "missing or invalid objectset file: " << objectset_file_node_name;
    }

    CHECK(objectset_file_node->file()->open(&objectset->objectset_file),
            Error::OPENFAILED) << "failed to open objectset file";

    auto l0_it = objectset->objectset_file->iterator();

    const wz::OpenedFile::Node* l0 = nullptr;
    while ((l0 = l0_it.next())) {
        auto l1_it = l0->iterator();

        const wz::OpenedFile::Node* l1 = nullptr;
        while ((l1 = l1_it.next())) {
            auto l2_it = l1->iterator();

            const wz::OpenedFile::Node* l2 = nullptr;
            while ((l2 = l2_it.next())) {
                gfx::Sprite sprite;
                // Load the sprite.
                CHECK(gfx::Sprite::loadfromfile(
                            &sprite,
                            l2),
                        Error::OBJECTSET_LOAD_SPRITELOADFAILED) << "failed to load object gfx sprite: "
                    << l0->name << "/" << l1->name << "/" << l2->name;

                Map::ObjectSet::Object object;

                CHECK(client::Sprite::init(
                            &object.sprite,
                            std::move(sprite),
                            &universe->time),
                        Error::OBJECTSET_LOAD_SPRITELOADFAILED) << "failed to load object sprite";

                const Map::ObjectSet::Object::Name name = {
                    .l0 = l0->name,
                    .l1 = l1->name,
                    .l2 = l2->name,
                };
                objectset->objects.emplace(name, std::move(object));
            }
        }
    }

    return Error();
}

static Error Map_load_layer(
        client::Universe* universe,
        Map* self,
        wz::Vfs* map_vfs,
        Map::Layer* layer,
        const wz::OpenedFile::Node* layer_node,
        Map::LoadResults* results) {
    // Load TileSet and tiles.
    const Map::TileSet* tileset = nullptr;
    do {
        const wz::OpenedFile::Node* tileset_name_node =
            layer_node->find(L"info/tS");
        if (tileset_name_node == nullptr || tileset_name_node->kind != wz::Property::STRING)
            break;

        const wchar_t* tileset_name_s = tileset_name_node->string;

        // Load the named tileset, if we haven't already.
        const std::wstring tileset_name(tileset_name_s);
        if (!self->tilesets.contains(tileset_name)) {
            Map::TileSet new_tileset;

            // It's not necessarily a fatal error to fail to load a tileset.
            Error e = TileSet_load(
                    map_vfs,
                    &new_tileset,
                    tileset_name_s,
                    results);
            if (e) {
                // Record the failed load and move on.
                if (results) {
                    std::wstringstream ss;
                    e.print(ss);

                    results->tilesets_missing[tileset_name] = ss.str();
                }

                break;
            }

            self->tilesets.emplace(tileset_name, std::move(new_tileset));
            tileset = &self->tilesets.at(tileset_name);
        } else {
            tileset = &self->tilesets.at(tileset_name);
        }
    } while (false);

    layer->tileset = tileset;

    if (tileset) {
        const wz::OpenedFile::Node* tile_node =
            layer_node->find(L"tile");
        if (tile_node != nullptr) {
            auto tile_it = tile_node->iterator();

            const wz::OpenedFile::Node* tile_node = nullptr;
            while ((tile_node = tile_it.next())) {
                // TODO: The errors here should probably not be errors, but just failed loads.
                Map::Layer::Tile tile;
                Map::TileSet::Tile::Name name;
                CHECK(tile_node->deserialize(
                            L"u", &name.u,
                            L"no", &name.no,
                            L"x", &tile.position.x,
                            L"y", &tile.position.y),
                        Error::LAYER_LOAD_MISSINGATTRIBUTES) << "missing or invalid tile property";

                // Specifically ignore errors here.
                int32_t zm = 0;
                tile_node->childint32(L"zM", &zm);

                // Lookup the tile in the tileset to compute the z.
                auto tileset_it = tileset->tiles.find(name);
                if (tileset_it == tileset->tiles.end()) {
                    return error_new(Error::LAYER_LOAD_MISSINGATTRIBUTES)
                        << "named tile not found in tileset";
                }
                tile.tile = &(tileset_it->second);

                tile.z = tileset_it->second.z + zm;
                layer->tiles.emplace_back(std::move(tile));
            }
        }

        // Sort tiles.
        std::sort(
                layer->tiles.begin(),
                layer->tiles.end(),
                [](const Map::Layer::Tile& left, const Map::Layer::Tile& right) {
                return left.z < right.z;
                });
    }

    // Load objects.
    const wz::OpenedFile::Node* objects_node = layer_node->find(L"obj");
    if (objects_node) {
        // Track the object sets that fail to load.
        // Since we attempt to load object sets on every object in the layer, we can repeatedly
        // try to load object sets that won't load.
        std::set<std::wstring> objectsets_missing;

        auto it = objects_node->iterator();

        const wz::OpenedFile::Node* object_node = nullptr;
        while ((object_node = it.next())) {
            // TODO: The errors here should probably not be errors, but just failed loads.
            Map::Layer::Object object;

            // First, look for this object's set.
            const wchar_t* objectset_name_s = nullptr;
            Map::ObjectSet::Object::Name name;
            CHECK(object_node->deserialize(
                        L"oS", &objectset_name_s,
                        L"l0", &name.l0,
                        L"l1", &name.l1,
                        L"l2", &name.l2,
                        L"x", &object.position.x,
                        L"y", &object.position.y,
                        L"z", &object.z,
                        L"f", &object.f),
                    Error::LAYER_LOAD_MISSINGATTRIBUTES)
                << "missing or invalid object attributes";

            const std::wstring objectset_name(objectset_name_s);
            if (objectsets_missing.contains(objectset_name))
                continue;

            const Map::ObjectSet* objectset = nullptr;
            if (!self->objectsets.contains(objectset_name)) {
                Map::ObjectSet new_objectset;

                // It's not necessarily a fatal error to fail to load an objectset.
                Error e = ObjectSet_load(
                        universe,
                        map_vfs,
                        &new_objectset,
                        objectset_name_s,
                        results);
                if (e) {
                    objectsets_missing.insert(objectset_name);

                    // Record the failed load and move on.
                    if (results) {
                        std::wstringstream ss;
                        e.print(ss);

                        results->objectsets_missing[objectset_name] = ss.str();
                    }

                    continue;
                }

                self->objectsets.emplace(objectset_name, std::move(new_objectset));
                objectset = &self->objectsets.at(objectset_name);
            } else {
                objectset = &self->objectsets.at(objectset_name);
            }

            auto objectset_it = objectset->objects.find(name);
            if (objectset_it == objectset->objects.end()) {
                if (results) {
                    std::wstringstream key_ss;
                    key_ss << layer_node->name << "/" << name.l0 << "/" << name.l1 << "/" << name.l2;

                    results->objects_missing[key_ss.str()] = L"not found in objectset";
                }

                continue;
            }
            object.object = &(objectset_it->second);

            layer->objects.emplace_back(std::move(object));
        }

        // Sort objects.
        std::sort(
                layer->objects.begin(),
                layer->objects.end(),
                [](const Map::Layer::Object& left, const Map::Layer::Object& right) {
                return left.z < right.z;
                });
    }

    return Error();
}

static Error Map_load_background(
        Map* self,
        wz::Vfs* map_vfs,
        Map::Background* background,
        const wz::OpenedFile::Node* back_node) {
    int32_t kind_i = 0;
    const wchar_t* back_file_node_basename = nullptr;
    int32_t no = 0;
    CHECK(back_node->deserialize(
                L"x", &background->position.x,
                L"y", &background->position.y,
                L"cx", &background->c.x,
                L"cy", &background->c.y,
                L"rx", &background->r.x,
                L"ry", &background->r.y,
                L"ani", &background->ani,
                L"type", &kind_i,
                L"front", &background->front,
                L"bS", &back_file_node_basename,
                L"no", &no),
            Error::BACKGROUND_LOAD_MISSINGATTRIBUTES)
        << "missing or invalid background property";

    // Optional.
    background->f = 0;
    back_node->childint32(L"f", &background->f);

    background->kind = static_cast<Map::Background::Kind>(kind_i);
    if (background->kind < 0 || background->kind >= Map::Background::KIND_COUNT) {
        return error_new(Error::BACKGROUND_LOAD_MISSINGATTRIBUTES)
            << "unknown background kind " << background->kind;
    }

    // Find the background file.
    wz::Vfs::Node* back_file_node = nullptr;
    std::wstring back_file_node_name;
    {
        std::wstringstream ss;
        ss << "Back/" << back_file_node_basename << ".img";
        back_file_node_name = ss.str();

        back_file_node = map_vfs->find(back_file_node_name.c_str());
    }
    if (back_file_node == nullptr || back_file_node->file() == nullptr) {
        return error_new(Error::BACKGROUND_LOAD_MISSINGFILE)
            << "missing or invalid background file: " << back_file_node_name;
    }

    // Open the background file.
    CHECK(back_file_node->file()->open(&background->background_file),
            Error::OPENFAILED) << "failed to open back file";

    // Load the background frame.
    std::wstring frame_node_name;

    // TODO: Properly handle animated backgrounds, including e.g. a0 and a1.
    // For now, we just load the first frame of the animated background.
    if (background->ani) {
        std::wstringstream ss;
        ss << "ani/" << no << "/0";

        frame_node_name = ss.str();
    } else {
        std::wstringstream ss;
        ss << "back/" << no;

        frame_node_name = ss.str();
    }

    const wz::OpenedFile::Node* frame_node = background->background_file->find(frame_node_name.c_str());
    if (frame_node == nullptr || frame_node->kind != wz::Property::CANVAS) {
        return error_new(Error::BACKGROUND_LOAD_MISSINGFRAME)
            << "missing or invalid frame";
    }

    CHECK(gfx::Sprite::Frame::load(
                &background->frame,
                frame_node->canvas.image,
                frame_node->canvas.image_data),
            Error::BACKGROUND_LOAD_FRAMELOADFAILED) << "failed to load frame";

    CHECK(frame_node->childvector(
                L"origin",
                &background->frame.origin.x,
                &background->frame.origin.y),
            Error::BACKGROUND_LOAD_MISSINGATTRIBUTES) << "failed to load frame origin";

    if (background->c.x == 0) {
        background->c.x = background->frame.image.width;
    }
    if (background->c.y == 0) {
        background->c.y = background->frame.image.height;
    }

    return Error();
}

Error Map::load(
        client::Universe* universe,
        Map* self,
        wz::Vfs* map_vfs,
        wz::Vfs::File::Handle&& map_file,
        Map::LoadResults* results) {
    self->map_file = std::move(map_file);

    self->time = universe->time.add<systems::Time::Component::Milliseconds>();

    // Load backgrounds.
    {
        const wz::OpenedFile::Node* backs =
            self->map_file->find(L"back");
        if (backs != nullptr) {
            wz::OpenedFile::Node::Iterator it = backs->iterator();

            const wz::OpenedFile::Node* back_node = nullptr;
            while ((back_node = it.next())) {
                Map::Background background;

                // Extract z value from nodename.
                if (util::convert(back_node->name, &background.z)) {
                    if (results)
                        results->backgrounds_missing[back_node->name] =
                            L"node name is not an integer";
                    continue;
                }

                // Load this background.
                Error e = Map_load_background(
                        self,
                        map_vfs,
                        &background,
                        back_node);
                if (e) {
                    if (results)
                        results->backgrounds_missing[back_node->name] =
                            e.str();
                    continue;
                }

                self->backgrounds.push_back(std::move(background));
                LOG(Logger::INFO)
                    << "loaded background " << back_node->name;
            }
        } else {
            if (results)
                results->backgrounds_missing[L"<all-backgrounds>"] =
                    L"missing or invalid back node";
        }
    }

    // Backgrounds should be sorted in render order.
    std::sort(
            self->backgrounds.begin(),
            self->backgrounds.end(),
            [](const Map::Background& b1, const Map::Background& b2) {
                return b1.z < b2.z;
                });

    // Load layers.
    {
        // For now, load specifically named layers [0, 7].
        for (size_t i = 0; i < 8; ++i) {
            const wz::OpenedFile::Node* layer_node = nullptr;
            {
                std::wstringstream ss;
                ss << i;
                layer_node = self->map_file->find(ss.str().c_str());
            }
            if (layer_node == nullptr)
                continue;

            Map::Layer layer;
            layer.index = i;
            CHECK(Map_load_layer(
                        universe,
                        self,
                        map_vfs,
                        &layer,
                        layer_node,
                        results),
                    Error::MAP_LOAD_LAYERLOADFAILED) << "failed to load layer " << i;

            self->layers.emplace_back(std::move(layer));
        }
    }

    // Load map info.
    const wz::OpenedFile::Node* info_node =
        self->map_file->find(L"info");
    if (info_node == nullptr) {
        return error_new(Error::OPENFAILED)
            << "map has no info node";
    }

    // Read the map's view envelope.
    struct {
        bool top;
        bool left;
        bool bottom;
        bool right;
    } has = {
        .top = true,
        .left = true,
        .bottom = true,
        .right = true,
    };

    if (info_node->childint32(L"VRTop", &self->bounding_box.topleft.y)) {
        has.top = false;
        self->bounding_box.topleft.y = std::numeric_limits<int32_t>::max();
    }
    if (info_node->childint32(L"VRLeft", &self->bounding_box.topleft.x)) {
        has.left = false;
        self->bounding_box.topleft.x = std::numeric_limits<int32_t>::max();
    }
    if (info_node->childint32(L"VRBottom", &self->bounding_box.bottomright.y)) {
        has.bottom = false;
        self->bounding_box.bottomright.y = std::numeric_limits<int32_t>::min();
    }
    if (info_node->childint32(L"VRRight", &self->bounding_box.bottomright.x)) {
        has.right = false;
        self->bounding_box.bottomright.x = std::numeric_limits<int32_t>::min();
    }

    // Deduce view envelope from layer construction, if no envelope is provided.
    if (!has.top || !has.left || !has.bottom || !has.right) {
        for (size_t i = 0, l = self->layers.size(); i < l; ++i) {
            const Map::Layer& layer = self->layers[i];

            for (size_t i = 0, l = layer.tiles.size(); i < l; ++i) {
                const Map::Layer::Tile& tile = layer.tiles[i];

                const gfx::Vector<int32_t> topleft = tile.position - tile.tile->frame.origin;
                const gfx::Vector<int32_t> bottomright = {
                    .x = topleft.x + static_cast<int32_t>(tile.tile->frame.image.width),
                    .y = topleft.y + static_cast<int32_t>(tile.tile->frame.image.height),
                };

                if (!has.top && topleft.y < self->bounding_box.topleft.y)
                    self->bounding_box.topleft.y = topleft.y;
                if (!has.left && topleft.x < self->bounding_box.topleft.x)
                    self->bounding_box.topleft.x = topleft.x;
                if (!has.bottom && bottomright.y > self->bounding_box.bottomright.y)
                    self->bounding_box.bottomright.y = bottomright.y;
                if (!has.right && bottomright.x > self->bounding_box.bottomright.x)
                    self->bounding_box.bottomright.x = bottomright.x;
            }

            for (size_t i = 0, l = layer.objects.size(); i < l; ++i) {
                const Map::Layer::Object& object = layer.objects[i];

                for (size_t i = 0, l = object.object->sprite.sprite.frames.size(); i < l; ++i) {
                    const gfx::Sprite::Frame& frame = object.object->sprite.sprite.frames[i];

                    const gfx::Vector<int32_t> topleft = object.position - frame.origin;
                    const gfx::Vector<int32_t> bottomright = {
                        .x = topleft.x + static_cast<int32_t>(frame.image.width),
                        .y = topleft.y + static_cast<int32_t>(frame.image.height),
                    };

                    if (!has.top && topleft.y < self->bounding_box.topleft.y)
                        self->bounding_box.topleft.y = topleft.y;
                    if (!has.left && topleft.x < self->bounding_box.topleft.x)
                        self->bounding_box.topleft.x = topleft.x;
                    if (!has.bottom && bottomright.y > self->bounding_box.bottomright.y)
                        self->bounding_box.bottomright.y = bottomright.y;
                    if (!has.right && bottomright.x > self->bounding_box.bottomright.x)
                        self->bounding_box.bottomright.x = bottomright.x;
                }
            }
        }
    }

    return Error();
}

}
