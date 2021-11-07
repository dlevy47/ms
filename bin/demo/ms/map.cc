#include "ms/map.hh"

#include <sstream>

#include "util/convert.hh"

namespace ms {

std::wstring Map::ID::padded() const {
    enum {
        MAPID_LENGTH = 9,
    };

    std::wstring padded_id;
    {
        std::wstringstream ss;
        ss.width(MAPID_LENGTH);
        ss.fill('0');
        ss << id;
        padded_id = ss.str();
    }

    return padded_id;
}

std::wstring Map::ID::as_filename() const {
    std::wstring padded_id = padded();

    std::wstringstream ss;
    ss << "Map/Map" << padded_id[0] << "/" << padded_id.c_str() << ".img";
    return ss.str();
}

bool Map::ID::from(
        ID* self,
        const wchar_t* name) {
    return (bool) util::convert(name, &self->id);
}

Error Map::load(
        Map* self,
        ID id,
        wz::Vfs::File::Handle&& map_file) {
    self->id = id;
    self->map_file = std::move(map_file);

    self->bounding_box.topleft.x = INT32_MAX;
    self->bounding_box.topleft.y = INT32_MAX;
    self->bounding_box.bottomright.x = INT32_MIN;
    self->bounding_box.bottomright.y = INT32_MIN;

    const wz::OpenedFile::Node* footholds =
        self->map_file->find(L"foothold");
    if (footholds != nullptr) {
        auto layer_it = footholds->iterator();

        const wz::OpenedFile::Node* layer_node = nullptr;
        while ((layer_node = layer_it.next())) {
            uint32_t layer_index = 0;
            CHECK(util::convert(layer_node->name, &layer_index),
                    Error::MAP_LOAD_LAYERLOADFAILED)
                << "layer node name is not an int32_t";

            Map::Layer* layer = &self->layers[layer_index];

            auto group_it = layer_node->iterator();

            const wz::OpenedFile::Node* group_node = nullptr;
            while ((group_node = group_it.next())) {
                uint32_t group_index = 0;
                CHECK(util::convert(group_node->name, &group_index),
                        Error::MAP_LOAD_LAYERLOADFAILED)
                    << "group node name is not an int32_t";

                auto foothold_it = group_node->iterator();

                const wz::OpenedFile::Node* foothold_node = nullptr;
                while ((foothold_node = foothold_it.next())) {
                    Map::Foothold foothold;
                    foothold.group = group_index;

                    CHECK(util::convert(foothold_node->name, &foothold.id),
                            Error::MAP_LOAD_LAYERLOADFAILED)
                        << "foothold node name is not an int32_t";

                    CHECK(foothold_node->deserialize(
                                L"x1", &foothold.start.x,
                                L"y1", &foothold.start.y,
                                L"x2", &foothold.end.x,
                                L"y2", &foothold.end.y,
                                L"prev", &foothold.prev_id,
                                L"next", &foothold.next_id),
                            Error::MAP_LOAD_FOOTHOLDLOADFAILED)
                        << "foothold property missing or invalid";

                    if (foothold.start.x < self->bounding_box.topleft.x)
                        self->bounding_box.topleft.x = foothold.start.x;
                    if (foothold.start.y < self->bounding_box.topleft.y)
                        self->bounding_box.topleft.y = foothold.start.y;
                    if (foothold.end.x > self->bounding_box.bottomright.x)
                        self->bounding_box.bottomright.x = foothold.end.x;
                    if (foothold.end.y > self->bounding_box.bottomright.y)
                        self->bounding_box.bottomright.y = foothold.end.y;

                    if (foothold.end.x < self->bounding_box.topleft.x)
                        self->bounding_box.topleft.x = foothold.end.x;
                    if (foothold.end.y < self->bounding_box.topleft.y)
                        self->bounding_box.topleft.y = foothold.end.y;
                    if (foothold.start.x > self->bounding_box.bottomright.x)
                        self->bounding_box.bottomright.x = foothold.start.x;
                    if (foothold.start.y > self->bounding_box.bottomright.y)
                        self->bounding_box.bottomright.y = foothold.start.y;

                    layer->footholds.emplace_back(std::move(foothold));
                }
            }
        }
    }

    const wz::OpenedFile::Node* ladders =
        self->map_file->find(L"ladderRope");
    if (ladders != nullptr) {
        auto ladder_it = ladders->iterator();

        const wz::OpenedFile::Node* ladder_node = nullptr;
        while ((ladder_node = ladder_it.next())) {
            Map::Ladder ladder;

            int32_t layer_index = 0;
            CHECK(ladder_node->childint32(L"page", &layer_index),
                    Error::MAP_LOAD_LADDERLOADFAILED)
                << "ladder page missing or invalid";

            if (layer_index < 0) {
                return error_new(Error::MAP_LOAD_LADDERLOADFAILED)
                    << "ladder page " << layer_index << " invalid";
            }

            Map::Layer* layer = &self->layers[static_cast<uint32_t>(layer_index)];

            int32_t x = 0;
            int32_t uf = 0;
            CHECK(ladder_node->deserialize(
                        L"uf", &uf,
                        L"x", &x,
                        L"y1", &ladder.start.y,
                        L"y2", &ladder.end.y),
                    Error::MAP_LOAD_LADDERLOADFAILED)
                << "ladder property missing or invalid";

            ladder.start.x = x;
            ladder.end.x = x;
            ladder.forbid_climb = uf == 0;

            layer->ladders.emplace_back(std::move(ladder));
        }
    }

    const wz::OpenedFile::Node* portals =
        self->map_file->find(L"portal");
    if (portals != nullptr) {
        auto portal_it = portals->iterator();

        const wz::OpenedFile::Node* portal_node = nullptr;
        while ((portal_node = portal_it.next())) {
            Map::Portal portal;

            int32_t kind = 0;
            CHECK(portal_node->deserialize(
                        L"pn", &portal.name,
                        L"pt", &kind,
                        L"x", &portal.at.x,
                        L"y", &portal.at.y,
                        L"tm", &portal.target_map.id,
                        L"tn", &portal.target_portal),
                    Error::MAP_LOAD_PORTALLOADFAILED)
                << "portal property missing or invalid";
            if (kind < 0 || kind > Map::Portal::COLLISIONCHANGEABLE) {
                return error_new(Error::MAP_LOAD_PORTALLOADFAILED)
                    << "portal kind " << kind << " invalid";
            }
            portal.kind = static_cast<Map::Portal::Kind>(kind);

            if (portal.at.x < self->bounding_box.topleft.x)
                self->bounding_box.topleft.x = portal.at.x;
            if (portal.at.y < self->bounding_box.topleft.y)
                self->bounding_box.topleft.y = portal.at.y;
            if (portal.at.x > self->bounding_box.bottomright.x)
                self->bounding_box.bottomright.x = portal.at.x;
            if (portal.at.y > self->bounding_box.bottomright.y)
                self->bounding_box.bottomright.y = portal.at.y;

            self->portals.emplace_back(std::move(portal));
        }
    }

    return Error();
}

}
