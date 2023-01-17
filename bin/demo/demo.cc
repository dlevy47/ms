#include "demo.hh"

#include "logger.hh"
#include "ms/map.hh"

Error Demo::draw_ui(gl::Window* window) {
    gfx::Vector<int> window_size;
    window->size(
        &window_size.x,
        &window_size.y);

    if (nk_begin(ui.context, "Find map", nk_rect(0, 0, window_size.x / 3, window_size.y / 2),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ui.context, 0, 2);
        nk_label(ui.context, "Map Name: ", NK_TEXT_RIGHT);

        int len = std::strlen(ui_state.map_name);
        int max_len = sizeof(ui_state.map_name) / sizeof(*ui_state.map_name) - 1;
        nk_edit_string(ui.context, NK_EDIT_FIELD, ui_state.map_name, &len, max_len, nullptr);
        ui_state.map_name[len] = 0;

        std::vector<ms::Map::ID> map_ids;
        if (len > 0) {
            std::wstring key = converter.from_bytes(ui_state.map_name);

            // TODO: Only update this on input change?
            map_ids = map_index.search(key.c_str());

            if (nk_tree_push(
                ui.context,
                NK_TREE_NODE,
                "map list",
                NK_MAXIMIZED)) {
                for (size_t i = 0; i < map_ids.size(); ++i) {
                    const ms::Map::ID id = map_ids[i];
                    std::string label;
                    {
                        std::wstringstream ss;
                        ss << "[" << id.padded() << "] " << map_index.id_to_name[id];
                        label = converter.to_bytes(ss.str().c_str());
                    }

                    nk_layout_row_begin(ui.context, NK_DYNAMIC, 0, 2); {
                        nk_layout_row_push(ui.context, 0.9);
                        nk_label(ui.context, label.c_str(), NK_TEXT_LEFT);

                        nk_layout_row_push(ui.context, 0.1);
                        if (nk_button_label(ui.context, ">")) {
                            CHECK(load_map(id),
                                Error::UIERROR)
                                << "failed to load map";
                        }
                    }
                    nk_layout_row_end(ui.context);
                }
            }
            nk_tree_pop(ui.context);
        }
    }
    nk_end(ui.context);

    if (nk_begin(ui.context, "Debug", nk_rect(window_size.x / 3, window_size.y / 2, window_size.x / 3, window_size.y / 3),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ui.context, 30, 1);

        map_state_options.debug.bounding_box =
            !nk_check_label(ui.context, "bounding boxes", !map_state_options.debug.bounding_box);
        map_state_options.debug.footholds =
            !nk_check_label(ui.context, "footholds", !map_state_options.debug.footholds);
        map_state_options.debug.ladders =
            !nk_check_label(ui.context, "ladders", !map_state_options.debug.ladders);
        map_state_options.debug.portals =
            !nk_check_label(ui.context, "portals", !map_state_options.debug.portals);
    }
    nk_end(ui.context);

    return Error();
}

Error Demo::init(
    Demo* self,
    const std::filesystem::path& dataset_path,
    uint64_t now,
    gfx::Vector<int32_t> window_size) {
    CHECK(client::Dataset::opendirectory(&self->dataset, dataset_path),
        Error::OPENFAILED) << "failed to load dataset";

    wz::Vfs::Node* map_node = self->dataset.string.vfs.find(L"Map.img");
    if (map_node == nullptr || map_node->file() == nullptr) {
        return error_new(Error::NOTFOUND)
            << "failed to find Map.img";
    }

    wz::Vfs::File::Handle map_file;
    CHECK(map_node->file()->open(&map_file),
        Error::OPENFAILED) << "failed to open Map.img";
    LOG(Logger::INFO)
        << "loaded Strings/Map.img";

    CHECK(ms::MapIndex::load(
        &self->map_index,
        std::move(map_file)),
        Error::UIERROR)
        << "failed to load map index";

    systems::Time::init(
        &self->universe.time,
        now);

    CHECK(client::renderer::MapLoader::init(
        &self->map_loader,
        &self->dataset,
        &self->universe),
        Error::UIERROR)
        << "failed to init map loader";
    LOG(Logger::INFO)
        << "inited map loader";

    CHECK(client::game::Renderer::init(
        &self->game_renderer),
        Error::UIERROR) << "failed to init client renderer";

    CHECK(nk::Ui::init(
        &self->ui),
        Error::UIERROR) << "failed to initialize nk ui";

    CHECK(client::ui::NkRenderer::init(
        &self->nk_renderer),
        Error::UIERROR) << "failed to initialize nk renderer";

    return Error();
}

Error Demo::load_map(
    ms::Map::ID map_id) {
    std::wstring map_filename = map_id.as_filename();
    LOG(Logger::INFO)
        << "looking for " << map_filename;

    // Extract the file.
    wz::Vfs::Node* map_node = dataset.map.vfs.find(map_filename.c_str());
    if (map_node == nullptr || map_node->file() == nullptr) {
        return error_new(Error::UIERROR)
            << "failed to find map file";
    }

    // Open the file.
    wz::Vfs::File::Handle map_file;
    CHECK(map_node->file()->open(&map_file),
        Error::OPENFAILED) << "failed to open map file";
    LOG(Logger::INFO)
        << "loaded map file";

    // Load the logical map.
    ms::Map map;
    CHECK(ms::Map::load(
        &map,
        map_id,
        std::move(map_file)),
        Error::OPENFAILED) << "failed to load logical map";
    LOG(Logger::INFO) << "logical map loaded";

    std::unique_ptr<ms::game::MapState> new_map_state =
        std::make_unique<ms::game::MapState>();

    CHECK(ms::game::MapState::init(
        new_map_state.get(),
        std::move(map)),
        Error::UIERROR) << "failed to load map state";
    map_state.swap(new_map_state);

    const wchar_t* name = map_index.name(map_id);
    if (name == nullptr) {
        name = L"(no map name found)";
    }
    LOG(Logger::INFO) << "loaded map " << name;

    return Error();
}
