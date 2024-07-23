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
        const auto engine = Engine::create(context);

        // Create a swap chain
        const auto swapChain = engine->createSwapChain();

        const auto buffer = VertexBuffer::Builder(3, 1)
            .binding(0, 4 * 3 + 4 * 4)
            .attribute(0, 0, AttributeFormat::Vec3, 0)
            .attribute(0, 1, AttributeFormat::Vec4, 4 * 3)
            .build(*engine);

        // The render loop
        context->loop([] {});

        // When we exit the loop, drawing and presentation operations may still be going on.
        // Cleaning up resources while that is happening is a bad idea.
        engine->waitIdle();

        // Destroy all resources
        engine->destroyVertexBuffer(buffer);
        engine->destroySwapChain(swapChain);
        engine->destroy();
    } catch (const std::exception& e) {
        PLOGE << e.what();
        return EXIT_FAILURE;
    }

    return 0;
}
