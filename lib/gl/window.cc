#include "gl/window.hh"

#include "logger.hh"

namespace gl {

void Window::char_callback(unsigned int codepoint) {
    key_codepoints.push(codepoint);
}

void Window::key_callback(int key, int scancode, int action, int mods) {
    if (action == GLFW_REPEAT) {
        return;
    }

    Keypress k = {
        .key = key,
        .action = action,
    };
    keys.push(k);
}

void Window::scroll_callback(double x, double y) {
    scroll.available = true;
    scroll.value.x = x;
    scroll.value.y = y;
}

bool Window::consume_scroll(gfx::Vector<double>* scroll) {
    if (this->scroll.available) {
        scroll->x = this->scroll.value.x;
        scroll->y = this->scroll.value.y;
        return true;
    }

    return false;
}

Error Window::frame(
        Window::Frame* f,
        gfx::Rect<uint32_t> viewport) {
    f->window = this;
    f->viewport = viewport;

    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    glViewport(
            viewport.topleft.x,
            viewport.topleft.y,
            viewport.bottomright.x - viewport.topleft.x,
            viewport.bottomright.y - viewport.topleft.y);

    return Error();
}

static void Window_char_callback(
        GLFWwindow* glfw_window,
        unsigned int codepoint) {
    Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));
    window->char_callback(codepoint);
}

static void Window_key_callback(
        GLFWwindow* glfw_window,
        int key,
        int scancode,
        int action,
        int mods) {
    Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));
    window->key_callback(key, scancode, action, mods);
}

static void Window_scroll_callback(
        GLFWwindow* glfw_window,
        double x,
        double y) {
    Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));
    window->scroll_callback(x, y);
}

Error Window::init(
        Window* w,
        const char* window_title) {
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    LOG(Logger::INFO) << "creating window";
    w->window = glfwCreateWindow(1200, 800, window_title, NULL, NULL);
    if (!w->window) {
        return error_new(Error::UIERROR)
            << "failed to create window";
    }
    glfwMakeContextCurrent(w->window);
    glfwSetCharCallback(w->window, Window_char_callback);
    glfwSetKeyCallback(w->window, Window_key_callback);
    glfwSetScrollCallback(w->window, Window_scroll_callback);
    glfwSetWindowUserPointer(w->window, w);
    LOG(Logger::INFO) << "window created";

    glfwSwapInterval(1);

    // TODO: If init is called twice, will it break?
    LOG(Logger::INFO) << "initializing glew";
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        return error_new(Error::UIERROR)
            << "failed to initialize glew: "
            << reinterpret_cast<const char*>(glewGetErrorString(err));
    }
    LOG(Logger::INFO) << "glew initialized";

    return Error();
}

void Window::size(int* width, int* height) const {
    glfwGetWindowSize(
            const_cast<GLFWwindow*>(window.get()),
            width,
            height);
}

void Window::framebuffer_size(int* width, int* height) const {
    glfwGetFramebufferSize(
            const_cast<GLFWwindow*>(window.get()),
            width,
            height);
}

}
