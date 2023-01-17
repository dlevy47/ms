#include "gl.hh"

#include "logger.hh"

namespace gl {

Error init() {
    LOG(Logger::INFO) << "initializing glfw";
    if (!glfwInit())
        return error_new(Error::UIERROR)
        << "failed to initialize glfw";
    LOG(Logger::INFO) << "glfw initialized";

    return Error();
}

}
