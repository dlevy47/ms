#include <sstream>
#include <string>
#include <vector>

#include "util/error.hh"

#include "browser.hh"
#include "gl.hh"

static void glfw_errorcallback(int code, const char* message) {
    std::cerr <<
        "GLFW error " << code << ": " << message << "\n";
}

Error main_(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return error_new(Error::INVALIDUSAGE)
            << "please provide a WZ filename";
    }

    glfwSetErrorCallback(glfw_errorcallback);

    CHECK(gl::init(),
        Error::UIERROR) << "failed to initialize gl";

    Browser browser;
    CHECK(Browser::init(
        &browser,
        args,
        "WZ browser"),
        Error::UIERROR) << "failed to initialize Browser";

    CHECK(browser.run(),
        Error::UIERROR) << "failed to run Browser";

    glfwTerminate();
    return Error();
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    Error e = main_(args);
    if (e) {
        std::cerr << "error\n";
        e.print(std::wcerr);
        return 1;
    }

    return 0;
}
