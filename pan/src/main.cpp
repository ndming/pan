#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Engine.h>

#include "Context.h"


int main(int argc, char* argv[]) {
    // Plant a logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    try {
        // Create a window context
        const auto context = Context::create("pan");

        // Create an engine and attach a surface to it
        const auto engine = Engine::create();
        engine->attachSurface(context->getNativeWindow());

        // Create a swap chain and a renderer
        const auto swapChain = engine->createSwapChain(context->getNativeWindow(), SwapChain::MSAA::x8);
        const auto renderer = engine->createRenderer(swapChain, Renderer::Pipeline::Particle);

        // The rendering loop
        context->loop([renderer, swapChain] (const double frameTimeSeconds) {
            renderer->compute(frameTimeSeconds);
            if (renderer->beginFrame(swapChain)) {
                renderer->render(swapChain);
                renderer->endFrame(swapChain);
            }
        });

        // When we exit the loop, drawing and presentation operations may still be going on.
        // This will ensure we are not cleaning up resources while those operations are happening.
        engine->flushAndWait();

        // Destroy all resources
        engine->destroyRenderer(renderer);
        engine->destroySwapChain(swapChain);
        engine->detachSurface();
        engine->destroy();
        context->destroy();
    } catch (const std::exception& e) {
        PLOGE << e.what();
        return EXIT_FAILURE;
    }

    return 0;
}
