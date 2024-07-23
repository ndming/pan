#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Context.h>
#include <engine/Engine.h>

#include <glm/glm.hpp>


struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
};

static constexpr auto vertices = std::array{
    Vertex{ { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
    Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
    Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } },
};

static constexpr auto positions = std::array{
    glm::vec3{ -0.5f, -0.5f, 0.0f },
    glm::vec3{ -0.5f, -0.5f, 0.0f },
    glm::vec3{ -0.5f, -0.5f, 0.0f },
};

static constexpr auto colors = std::array{
    glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f },
    glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f },
    glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f },
};

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

        const auto buffer = VertexBuffer::Builder(2)
            .vertexCount(vertices.size())
            .binding(0, sizeof(glm::vec3))
            .binding(1, sizeof(glm::vec4))
            .attribute(0, 0, AttributeFormat::Vec3)
            .attribute(1, 1, AttributeFormat::Vec4)
            .build(*engine);

        buffer->setBindingData(0, positions.data(), *engine);
        buffer->setBindingData(1, colors.data(), *engine);

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
