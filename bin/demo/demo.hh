#pragma once

#include <codecvt>
#include <filesystem>
#include <locale>
#include <memory>
#include <optional>

#include "gl.hh"
#include "client/dataset.hh"
#include "client/game/renderer.hh"
#include "client/renderer/mapstate.hh"
#include "client/ui/nkrenderer.hh"
#include "client/universe.hh"
#include "gfx/rect.hh"
#include "gfx/vector.hh"
#include "ms/mapindex.hh"
#include "ms/game/mapstate.hh"
#include "nk/ui.hh"
#include "ui/input.hh"
#include "util/error.hh"

struct Demo {
    enum {
        CLIENT_NK = 1,
        CLIENT_MAP,
    };
    
    // converter is a converter from std::wstring to a utf8 std::string.
    // nuklear methods (and some OS interfaces) typically require a utf8
    // string, instead of a wchar_t string.
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    // dataset is the dataset to use for this demo.
    client::Dataset dataset;

    // universe is the universe used to simulate the map.
    client::Universe universe;

    // map_index is used to find map names.
    ms::MapIndex map_index;

    // map_loader is used to load resources (graphics, etc) for the map.
    client::renderer::MapLoader map_loader;

    // map_state is the current state of the map.
    std::unique_ptr<ms::game::MapState> map_state;

    // game_renderer is used to render to a gl::Window target.
    client::game::Renderer game_renderer;

    // map_state_renderer renders a provided map state.
    client::renderer::MapState map_state_renderer;

    // map_state_renderer_options contains the draw options for the map state.
    client::renderer::MapState::Options map_state_options{
        .debug = {
            .bounding_box = true,
            .footholds = true,
            .grid = true,
            .ladders = true,
            .portals = true,
        },
    };

    // map_viewport is the map coordinate rectangle to draw.
    gfx::Rect<double> map_viewport;

    // input is used to multiplex inputs to the map or UI.
    ui::Input input;

    client::ui::NkRenderer nk_renderer;

    nk::Ui ui;

    struct {
        char map_name[256]{ 0 };
    } ui_state;

    void processinput(
        gl::Window* window,
        const client::game::Renderer::Target* target);

    Error loadmap(
        ms::Map::ID map_id);

    Error drawui(gl::Window* window);

    static Error init(
        Demo* self,
        const std::filesystem::path& dataset_path,
        uint64_t now,
        gfx::Vector<int32_t> window_size);
};
