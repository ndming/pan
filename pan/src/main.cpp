#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Context.h>
#include <engine/Engine.h>


int main(int argc, char* argv[]) {
    // Plant a logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    // Create a window context
    const auto context = Context::create("pan");

    // Create an engine
    const auto engine = Engine::create();
    engine->attachSurface(context->getNativeWindow());

    // Destroy all resources
    engine->detachSurface();
    engine->destroy();
    context->destroy();

    return 0;
}
