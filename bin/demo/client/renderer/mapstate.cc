#include "client/renderer/mapstate.hh"

#include <string>

#include "logger.hh"

namespace client {
namespace renderer {

Error MapHelper::load(
    MapHelper* self,
    client::Map::Helper&& helper,
    systems::Time* time) {
    self->map_helper_file = std::move(helper.map_helper_file);

    for (auto& p : helper.editor_portal_sprites) {
        client::Sprite sprite;

        CHECK(client::Sprite::init(
            &sprite,
            std::move(p.second),
            time),
              Error::UIERROR)
        << "failed to load client sprite";

        self->portal_sprites.emplace(p.first, std::move(sprite));
    }

    for (auto& p : helper.game_portal_sprites) {
        client::Sprite sprite;

        CHECK(client::Sprite::init(
            &sprite,
            std::move(p.second),
            time),
              Error::UIERROR)
        << "failed to load client sprite";

        // Erase for p.first first, since emplace doesn't
        // replace.
        self->portal_sprites.erase(p.first);
        self->portal_sprites.emplace(p.first, std::move(sprite));
    }

    return Error();
}

Error MapLoader::init(
    MapLoader* loader,
    client::Dataset* dataset,
    client::Universe* universe) {
    loader->dataset = dataset;
    loader->universe = universe;

    wz::Vfs::Node* map_helper_node = dataset->map.vfs.find(L"MapHelper.img");
    if (map_helper_node == nullptr || map_helper_node->file() == nullptr) {
        return error_new(Error::UIERROR)
            << "failed to find MapHelper.img";
    }

    wz::Vfs::File::Handle map_helper_file;
    CHECK(map_helper_node->file()->open(&map_helper_file),
        Error::OPENFAILED) << "failed to open MapHelper.img";
    LOG(Logger::INFO)
        << "loaded MapHelper.img";

    client::Map::Helper client_map_helper;
    CHECK(client::Map::Helper::load(
        &client_map_helper,
        std::move(map_helper_file)),
        Error::UIERROR)
        << "failed to load client map helper";

    LOG(Logger::INFO)
        << "loading map helper";
    CHECK(MapHelper::load(
        &loader->map_helper,
        std::move(client_map_helper),
        &universe->time),
          Error::UIERROR)
    << "failed to load map helper";

    return Error();
}

Error MapLoader::load(
    client::Map** map,
    ms::Map::ID id) {
    auto it = maps.find(id);
    if (it != maps.end()) {
        *map = &it->second;
        return Error();
    }

    std::wstring map_filename = id.as_filename();

    // Extract the file.
    wz::Vfs::Node* map_node = dataset->map.vfs.find(map_filename.c_str());
    if (map_node == nullptr || map_node->file() == nullptr) {
        return error_new(Error::UIERROR)
            << "failed to find map file " << map_filename;
    }

    // Open the file.
    wz::Vfs::File::Handle map_file;
    CHECK(map_node->file()->open(&map_file),
        Error::OPENFAILED) << "failed to open map file";
    LOG(Logger::INFO)
        << "loaded map file";

    // Load the client map.
    client::Map::LoadResults load_results;
    client::Map new_map;
    CHECK(client::Map::load(
        universe,
        &new_map,
        &dataset->map.vfs,
        std::move(map_file),
        &load_results),
        Error::OPENFAILED) << "failed to load map";

    maps.emplace(id, std::move(new_map));
    *map = &maps.at(id);

    return Error();
}

static bool background_tileshorizontally(
    const client::Map::Background* background) {
    switch (background->kind) {
    case client::Map::Background::TILEDHORIZONTAL:
    case client::Map::Background::TILEDBOTH:
    case client::Map::Background::TILEDHORIZONTALMOVINGHORIZONTAL:
    case client::Map::Background::TILEDBOTHMOVINGHORIZONTAL:
    case client::Map::Background::TILEDBOTHMOVINGVERTICAL:
        return true;
    default:
        return false;
    }
}

static bool background_tilesvertically(
    const client::Map::Background* background) {
    switch (background->kind) {
    case client::Map::Background::TILEDVERTICAL:
    case client::Map::Background::TILEDBOTH:
    case client::Map::Background::TILEDVERTICALMOVINGVERTICAL:
    case client::Map::Background::TILEDBOTHMOVINGHORIZONTAL:
    case client::Map::Background::TILEDBOTHMOVINGVERTICAL:
        return true;
    default:
        return false;
    }
}

static bool background_moveshorizontally(
    const client::Map::Background* background) {
    switch (background->kind) {
    case client::Map::Background::TILEDHORIZONTALMOVINGHORIZONTAL:
    case client::Map::Background::TILEDBOTHMOVINGHORIZONTAL:
        return true;
    default:
        return false;
    }
}

static bool background_movesvertically(
    const client::Map::Background* background) {
    switch (background->kind) {
    case client::Map::Background::TILEDVERTICALMOVINGVERTICAL:
    case client::Map::Background::TILEDBOTHMOVINGVERTICAL:
        return true;
    default:
        return false;
    }
}

static Error background_stationary(
    const MapState* that,
    client::game::Renderer::Target* target,
    const client::Map::Background* background,
    const gfx::Vector<double> shift,
    const gfx::Rect<int32_t> bounds) {
    uint32_t row_count = 1;
    uint32_t column_count = 1;

    gfx::Vector<int32_t> start = background->position + shift;

    if (background_tileshorizontally(background)) {
        column_count = 3 +
            (bounds.width() / background->frame.image.width);

        start.x = bounds.topleft.x -
            (background->c.x -
                ((start.x - bounds.topleft.x) % background->c.x));
    }
    if (background_tilesvertically(background)) {
        row_count = 3 +
            (bounds.height() / background->frame.image.height);

        start.y = bounds.topleft.y -
            (background->c.y -
                ((start.y - bounds.topleft.y) % background->c.y));
    }

    gfx::Vector<int32_t> at = start;
    for (uint32_t row = 0; row < row_count; ++row) {
        for (uint32_t column = 0; column < column_count; ++column) {
            target->frame(
                &background->frame,
                at);

            at.x += background->c.x;
        }

        at.x = start.x;
        at.y += background->c.y;
    }

    return Error();
}

static Error background(
    const MapState* that,
    client::game::Renderer::Target* target,
    const client::Map::Background* background,
    const ms::game::MapState* state,
    const client::Map* resources,
    uint64_t time) {
    // shift is the amount to modify this background's position, based on
    // computed parallax. It's expressed in the data as a percentage of
    // the background's distance from the center of the map.
    gfx::Vector<double> shift = {
        .x = target->game_viewport.topleft.x + target->game_viewport.width() / 2,
        .y = target->game_viewport.topleft.y + target->game_viewport.height() / 2,
    };

    // For moving background, compute the movement by computing the time
    // interval in which the background should complete a full tile cycle.
    // This time interval is r * 5 seconds.

    shift.x += background->r.x * (target->game_viewport.topleft.x + target->game_viewport.width() / 2) / 100;
    if (background_moveshorizontally(background)) {
        uint64_t interval = background->r.x * 15000;
        shift.x += static_cast<double>(time % interval) / interval * background->c.x;
    }

    shift.y += background->r.y * (target->game_viewport.topleft.y + target->game_viewport.height() / 2) / 100;
    if (background_movesvertically(background)) {
        uint64_t interval = background->r.y * 15000;
        shift.y += static_cast<double>(time % interval) / interval * background->c.y;
    }

    switch (background->kind) {
    case client::Map::Background::SINGLE:
    case client::Map::Background::TILEDHORIZONTAL:
    case client::Map::Background::TILEDVERTICAL:
    case client::Map::Background::TILEDBOTH:

    case client::Map::Background::TILEDHORIZONTALMOVINGHORIZONTAL:
    case client::Map::Background::TILEDVERTICALMOVINGVERTICAL:
    case client::Map::Background::TILEDBOTHMOVINGHORIZONTAL:
    case client::Map::Background::TILEDBOTHMOVINGVERTICAL:
        return background_stationary(
            that,
            target,
            background,
            shift,
            resources->bounding_box);
    default:
        // TODO
        break;
    }

    return Error();
}

static Error layer(
    const MapState* that,
    client::game::Renderer::Target* target,
    const client::Map::Layer* layer) {
    for (const client::Map::Layer::Object& object : layer->objects) {
        target->frame(
            object.object->sprite.frame(),
            object.position);
    }

    for (const client::Map::Layer::Tile& tile : layer->tiles) {
        target->frame(
            &tile.tile->frame,
            tile.position);
    }

    return Error();
}

Error MapState::render(
    const MapState::Options* options,
    client::game::Renderer::Target* target,
    MapLoader* loader,
    const ms::game::MapState* state,
    uint64_t now) const {
    // First, we need to get the resources for this map, which has the graphical info.
    client::Map* resources;
    CHECK(loader->load(
        &resources,
        state->basemap.id),
        Error::RESOURCELOADFAILED)
        << "failed to load map resources";

    target->that->program.viewport(
        (gfx::Rect<double>) resources->bounding_box);

    // Draw the backgrounds before anything else.
    uint32_t i = 0;
    for (const client::Map::Background& b : resources->backgrounds) {
        background(
            this,
            target,
            &b,
            state,
            resources,
            now);
        ++i;
    }

    for (const client::Map::Layer& l : resources->layers) {
        layer(
            this,
            target,
            &l);
    }

    if (options->debug.portals) {
        for (const ms::Map::Portal& p : state->basemap.portals) {
            auto it = loader->map_helper.portal_sprites.find(p.kind);
            if (it == loader->map_helper.portal_sprites.end())
                continue;

            target->frame(
                it->second.frame(),
                p.at);
        }
    }

    if (options->debug.footholds) {
        for (const auto& [k, l] : state->basemap.layers) {
            for (const ms::Map::Foothold& f : l.footholds) {
                target->line(
                    f.start,
                    f.end);
            }

            for (const ms::Map::Ladder& l : l.ladders) {
                target->line(
                    l.start,
                    l.end);
            }
        }
    }

    if (options->debug.bounding_box) {
        client::game::Renderer::Target::LineOptions line_options = {
            .color = {
                .r = 255,
                .g = 0,
                .b = 0,
            },
        };

        target->rect_withoptions(
            state->basemap.bounding_box,
            line_options);

        line_options.color.g = 255;
        target->rect_withoptions(
            resources->bounding_box,
            line_options);
    }

    if (options->debug.grid) {
        client::game::Renderer::Target::LineOptions line_options = {
            .width = 2,
            .color = {
                .r = 0,
                .g = 200,
                .b = 0,
                .a = 100,
            },
        };

        // Axes.
        if (state->basemap.bounding_box.topleft.x < 0 && state->basemap.bounding_box.bottomright.x > 0) {
            // Vertical axis.
            target->line_withoptions(
                {
                    .x = 0,
                    .y = state->basemap.bounding_box.topleft.y,
                },
                {
                    .x = 0,
                    .y = state->basemap.bounding_box.bottomright.y,
                },
                line_options);
        }

        if (state->basemap.bounding_box.topleft.y < 0 && state->basemap.bounding_box.bottomright.y > 0) {
            // Horizontal axis.
            target->line_withoptions(
                {
                    .x = state->basemap.bounding_box.topleft.x,
                    .y = 0,
                },
                {
                    .x = state->basemap.bounding_box.bottomright.x,
                    .y = 0,
                },
                line_options);
        }

        // Grid.
        const int32_t increment = 100;
        line_options.width = 1;
        line_options.color.b = 100;
        
        // Horizontal.
        for (int32_t y = (state->basemap.bounding_box.topleft.y / increment) * increment; y < state->basemap.bounding_box.bottomright.y; y += increment) {
            if (y != 0) {
                target->line_withoptions(
                    {
                        .x = state->basemap.bounding_box.topleft.x,
                        .y = y,
                    },
                    {
                        .x = state->basemap.bounding_box.bottomright.x,
                        .y = y,
                    },
                    line_options);
            }
        }
        
        // Vertical.
        for (int32_t x = (state->basemap.bounding_box.topleft.x / increment) * increment; x < state->basemap.bounding_box.bottomright.x; x += increment) {
            if (x != 0) {
                target->line_withoptions(
                    {
                        .x = x,
                        .y = state->basemap.bounding_box.topleft.y,
                    },
                    {
                        .x = x,
                        .y = state->basemap.bounding_box.bottomright.y,
                    },
                    line_options);
            }
        }
    }

    return Error();
}

}
}
