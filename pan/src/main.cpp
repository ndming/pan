#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Context.h>
#include <engine/Engine.h>


int main(int argc, char* argv[]) {
    // Plant a console logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    try {
        // Create a window context
        const auto context = Context::create("pan");

        // Create an engine
        const auto engine = Engine::create();

        // Create a swap chain
        const auto swapChain = engine->createSwapChain(context);

        // Destroy all resources
        engine->destroySwapChain(swapChain);
        engine->destroy();
        context->destroy();
    } catch (const std::exception& e) {
        PLOGE << e.what();
        return EXIT_FAILURE;
    }

    return 0;
}
