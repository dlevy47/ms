#pragma once

#include <codecvt>
#include <memory>
#include <locale>

#include "wz/vfs.hh"
#include "wz/wz.hh"

#include "nk.hh"
#include "client/game/renderer.hh"
#include "client/ui/nkrenderer.hh"
#include "gfx/sprite.hh"
#include "gfx/vector.hh"
#include "gl/window.hh"
#include "nk/ui.hh"

struct BrowsedFile {
    std::string filename;
    wz::Wz wz;
    wz::Vfs vfs;

    BrowsedFile():
        filename(),
        wz(),
        vfs() {}

    BrowsedFile(BrowsedFile&& rhs):
        filename(std::move(rhs.filename)),
        wz(std::move(rhs.wz)),
        vfs(std::move(rhs.vfs)) {}
};

struct Browser {
    // converter is a converter from std::wstring to a utf8 std::string.
    // nuklear methods (and some OS interfaces) typically require a utf8
    // string, instead of a wchar_t string.
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    // browsed_files is the list of BrowsedFiles opened in this Browser.
    std::vector<BrowsedFile> browsed_files;

    // frame is the frame used to show a selected image.
    std::unique_ptr<gfx::Sprite::Frame> frame;

    gl::Window window;

    client::game::Renderer game_renderer;

    client::ui::NkRenderer nk_renderer;

    // ui is the nuklear UI to render.
    nk::Ui ui;

    static Error init(
            Browser* b,
            const std::vector<std::string>& argv,
            const char* window_title);

    Error run();

    Browser() = default;
    Browser(const Browser&) = delete;
};
