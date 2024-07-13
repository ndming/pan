#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Engine.h>

#include "Context.h"


int main(int argc, char* argv[]) {
    // Plant a logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    // Create a window context
    const auto context = Context::create("pan");

    // Create an engine and attach a surface to it
    const auto engine = Engine::create();
    engine->attachSurface(context->getNativeWindow());

    // Create a swap chain
    auto swapChain = engine->createSwapChain(context->getNativeWindow());

    // Destroy all resources
    engine->destroySwapChain(std::move(swapChain));
    engine->detachSurface();
    engine->destroy();
    context->destroy();

    return 0;
}
