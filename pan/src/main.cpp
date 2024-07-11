#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Engine.h>


int main(int argc, char* argv[]) {
    // Plant a logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    // Create a window
    glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const auto window = glfwCreateWindow(1280, 768, "pan", nullptr, nullptr);

    // Create an engine
    const auto engine = Engine::create();
    engine->bindSurface(window, {}, true);

    // Destroy all resources
    engine->destroy();

    // Destroy the window
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
