#pragma once

#include <optional>
#include <queue>

#include "gl.hh"
#include "p.hh"
#include "gfx/rect.hh"
#include "gfx/vector.hh"
#include "util/error.hh"

namespace gl {

struct Window {
    struct Frame {
        P<Window> window;

        gfx::Rect<uint32_t> viewport;

        ~Frame() {
            if (window) {
                window->resize.reset();
                window->scroll.available = false;

                glfwSwapBuffers(window->window);
                glfwPollEvents();
            }
        }

        Frame() = default;
        Frame(Frame&&) = default;
        Frame(const Frame&) = delete;
    };
    
    // window is this Window's GLFW window.
    P<GLFWwindow> window;

    // scroll is a maybe updated scroll value from the glfw scroll callback,
    // and a boolean indicating whether a new value is available.
    struct {
        gfx::Vector<double> value;
        bool available{ false };
    } scroll;

    // resize is a possibly updated new size for the window.
    std::optional<gfx::Vector<int>> resize;

    struct Keypress {
        int key{ 0 };
        int action{ GLFW_PRESS };
    };

    std::queue<Keypress> keys;

    std::queue<unsigned int> key_codepoints;

    struct MouseButton {
        enum {
            PRESS,
            RELEASE,
        } action;

        int32_t button;
    };

    std::queue<MouseButton> mouse_buttons;

    static Error init(
        Window* r,
        const char* window_title);

    // size gets the current window size.
    void size(int* width, int* height) const;

    // framebuffer_size gets the current framebuffer size.
    void framebuffer_size(int* width, int* height) const;

    // scroll_callback is intended to be called from a glfw scroll callback.
    void scroll_callback(double x, double y);

    // key_callback is intended to be called from a glfw key callback.
    void key_callback(int key, int scancode, int action, int mods);

    // char_callback is intended to be called from a glfw character callback.
    void char_callback(unsigned int codepoint);

    // mouse_button_callback is intended to be called from a glfw mouse button callback.
    void mouse_button_callback(int button, int action, int mods);

    // resize_callback is intended to be called from a glfw window size callback.
    void resize_callback(int width, int height);

    // consume_scroll fetches the value of the current scroll, if one is
    // available.
    // TODO: Rename this to just `scroll`.
    bool consume_scroll(gfx::Vector<double>* scroll);

    // frame starts a new rendering frame.
    Error frame(
        Frame* f,
        gfx::Rect<uint32_t> viewport);

    // shouldclose returns whether or not this window has requested to close.
    bool shouldclose() {
        return glfwWindowShouldClose(window);
    }

    Window() = default;

    // The move constructor is deleted to enforce that this type isn't
    // moved or copied, since a pointer to the Window is stored in
    // the glfw window.
    Window(Window&&) = delete;
    Window(const Window&) = delete;
};

}
