#include <chrono>
#include <iostream>
#include <sstream>

#include "logger.hh"
#include "client/dataset.hh"
#include "client/map.hh"
#include "client/time.hh"
#include "client/universe.hh"
#include "client/game/renderer.hh"
#include "client/renderer/mapstate.hh"
#include "gl/window.hh"
#include "ms/map.hh"
#include "ms/game/mapstate.hh"
#include "systems/time.hh"
#include "util/convert.hh"
#include "util/error.hh"
#include "wz/vfs.hh"
#include "gl.hh"

#include "oui-blendish/blendish.h"

#include "demo.hh"

enum {
    MAPID_LENGTH = 9,
};

static void draw_blendish() {
}

static void glfw_errorcallback(int code, const char* message) {
    LOG(Logger::ERROR) <<
        "GLFW error " << code << ": " << message;
}

Error main_(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        return error_new(Error::INVALIDUSAGE)
            << "please provide a WZ directory and map ID";
    }

    glfwSetErrorCallback(glfw_errorcallback);

    CHECK(gl::init(),
            Error::UIERROR) << "failed to initialize gl";

    gl::Window window;
    CHECK(gl::Window::init(
                &window,
                "Map demo"),
            Error::UIERROR) << "failed to initialize window";

    // Convert the provided map ID to an integer so that we can
    // convert it to a properly sized string below.
    ms::Map::ID map_id;
    {
        std::stringstream ss(args[2]);
        ss >> map_id.id;
    }
    if (map_id.id == 0) {
        return error_new(Error::INVALIDUSAGE)
            << "invalid map id specified";
    }

    gfx::Vector<int> framebuffer_size;
    window.framebuffer_size(
            &framebuffer_size.x,
            &framebuffer_size.y);

    Demo demo;
    CHECK(Demo::init(
                &demo,
                args[1],
                client::now().to_milliseconds(),
                framebuffer_size),
            Error::UIERROR)
        << "failed to initialize demo";

    if (args.size() > 3) {
        // TODO: Delete this.
        // Lookup a provided string in the map index.
        std::wstring key;
        {
            std::wstringstream ss;
            ss << args[3].c_str();
            key = ss.str();
        }

        std::vector<ms::Map::ID> ids =
            demo.map_index.search(key.c_str());

        for (auto it = ids.begin(); it != ids.end(); it++) {
            LOG(Logger::INFO) << "  matched map: " <<
                demo.map_index.name(*it);
        }
    }

    CHECK(demo.load_map(map_id),
            Error::UIERROR)
        << "failed to load map " << map_id;
    {
        // Set initial viewport.
        gfx::Vector<int> window_size;

        window.size(
                &window_size.x,
                &window_size.y);

        // The map viewport starts as a rectangle the same size as the window, centered on the
        // center of the map.
        gfx::Rect<int32_t> starting_viewport = {0};
        starting_viewport.bottomright = window_size;

        starting_viewport.translate(
                demo.map_state->basemap.bounding_box.center() - starting_viewport.center());
        demo.map_viewport = (gfx::Rect<double>) starting_viewport;
    }

    auto last_metrics = std::chrono::system_clock::now();

    while (!window.shouldclose()) {
        gfx::Vector<int> window_size;
        gfx::Vector<int> display_size;

        window.size(
                &window_size.x,
                &window_size.y);
        window.framebuffer_size(
                &display_size.x,
                &display_size.y);

        struct {
            struct {
                gfx::Vector<double> now;
                gfx::Vector<double> last;
            } screen;

            struct {
                gfx::Vector<double> now;
                gfx::Vector<double> last;
            } ndc;
        } mouse;

        {
            glfwGetCursorPos(
                    window.window,
                    &mouse.screen.now.x,
                    &mouse.screen.now.y);

            // Mouse positions are in screen space. First, project them to NDC.
            mouse.ndc.now.x = ((mouse.screen.now.x * 2) / window_size.x) - 1;
            mouse.ndc.now.y = 1 - ((mouse.screen.now.y * 2) / window_size.y);

            mouse.ndc.last.x = ((mouse.screen.last.x * 2) / window_size.x) - 1;
            mouse.ndc.last.y = 1 - ((mouse.screen.last.y * 2) / window_size.y);
        }

        bool drag = glfwGetMouseButton(window.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        if (nk_window_is_any_hovered(demo.ui.context)) {
            drag = false;
        }

        gfx::Rect<uint32_t> window_viewport = {
            .topleft = {0},
            .bottomright = (gfx::Vector<uint32_t>) display_size,
        };

        {
            gl::Window::Frame frame;
            CHECK(window.frame(
                        &frame,
                        window_viewport),
                    Error::UIERROR) << "failed to start frame";

            {
                client::game::Renderer::Target target =
                    demo.game_renderer.begin(
                            &frame,
                            demo.map_viewport);

                gfx::Vector<double> map_tmp = target.unproject(mouse.ndc.now);
                if (drag) {
                    gfx::Vector<double> map_lmp = target.unproject(mouse.ndc.last);

                    gfx::Vector<double> translate = map_lmp - map_tmp;
                    demo.map_viewport.translate(translate);
                }

                gfx::Vector<double> scroll;
                if (window.consume_scroll(&scroll)) {
                    if (scroll.y > 10) scroll.y = 10;
                    if (scroll.y < -10) scroll.y = -10;

                    scroll.y *= -.04;
                    scroll.y += 1;

                    // Limit area of the viewport to cap zoom.
                    if (scroll.y < 1 && demo.map_viewport.area() < (0.01 * demo.map_state->basemap.bounding_box.area())) {
                        // Area is too small, and wants to be zoomed in.
                    } else if (scroll.y > 1 && demo.map_viewport.area() > (10 * demo.map_state->basemap.bounding_box.area())) {
                        // Area is too big, and wants to be zoomed out.
                    } else {
                        demo.map_viewport.scale_about(scroll.y, map_tmp);
                    }
                }

                client::renderer::MapState::Options options;
                options.debug.portals = true;
                options.debug.footholds = true;
                options.debug.ladders = true;
                options.debug.bounding_box = true;
                CHECK(demo.map_state_renderer.render(
                            &options,
                            &target,
                            &demo.map_loader,
                            demo.map_state.get(),
                            demo.universe.time.last),
                        Error::UIERROR) << "failed to render map";

                {
                    using namespace std::literals;

                    if (std::chrono::system_clock::now() - last_metrics > 3s) {
                        last_metrics = std::chrono::system_clock::now();

                        LOG(DEBUG)
                            << "[render stats] "
                            << "quads: " << target.metrics.quads << " "
                            << "textures: " << target.metrics.textures() << " "
                            << "draw calls: " << target.metrics.draw_calls;

                        auto open_files = demo.dataset.openfiles();
                        std::wcerr << "open files: \n";
                        for (const auto& name : open_files) {
                            std::wcerr << "  " << name << "\n";
                        }
                    }
                }
            }

            demo.ui.input(
                    &window,
                    nk_window_is_any_hovered(demo.ui.context));

            CHECK(demo.draw_ui(&window),
                    Error::UIERROR)
                << "failed to draw demo ui";

            demo.nk_renderer.render(
                    &frame,
                    window_size,
                    &demo.ui);
        }

        {
            draw_blendish();
        }

        mouse.screen.last = mouse.screen.now;

        demo.universe.tick();
    }

    glfwTerminate();
    return Error();
}

int main(int argc, char* argv[]) {
    Logger::Global().targets.push_back(&std::wcerr);

    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    Error e = main_(args);
    if (e) {
        LOG(Logger::ERROR)
            << "error: " << e;
        return 1;
    }

    return 0;
}
