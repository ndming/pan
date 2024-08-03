#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Engine.h>
#include <engine/GraphicShader.h>
#include <engine/IndexBuffer.h>
#include <engine/VertexBuffer.h>
#include <engine/Scene.h>
#include <engine/View.h>

#include <glm/glm.hpp>


struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
};

static constexpr auto vertices = std::array{
    Vertex{ { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } },
    Vertex{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } },
    Vertex{ {  0.0f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } },
};

static constexpr auto positions = std::array{
    glm::vec3{ -0.5f, -0.5f, 0.0f },
    glm::vec3{  0.5f, -0.5f, 0.0f },
    glm::vec3{  0.0f,  0.5f, 0.0f },
};

static constexpr auto colors = std::array{
    glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f },
    glm::vec4{ 0.0f, 1.0f, 0.0f, 0.0f },
    glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f },
};

static constexpr auto indices = std::array<uint16_t, 3>{ 0, 1, 2 };

int main(int argc, char* argv[]) {
    // Plant a console logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    try {
        // Create a window context
        const auto context = Context::create("pan");

        // Create an engine
        const auto engine = Engine::create(context->getSurface());

        // Create a swap chain and a view
        const auto swapChain = engine->createSwapChain();
        const auto view = View::create(*swapChain);

        const auto vertexBuffer = VertexBuffer::Builder(2)
            .vertexCount(vertices.size())
            .binding(0, sizeof(glm::vec3))
            .binding(1, sizeof(glm::vec4))
            .attribute(0, 0, AttributeFormat::Vec3)
            .attribute(1, 1, AttributeFormat::Vec4)
            .build(*engine);
        vertexBuffer->setBindingData(0, positions.data(), *engine);
        vertexBuffer->setBindingData(1, colors.data(), *engine);

        const auto indexBuffer = IndexBuffer::Builder()
            .indexCount(indices.size())
            .indexType(IndexType::Uint16)
            .build(*engine);
        indexBuffer->setData(indices.data(), *engine);

        const auto shader = GraphicShader::Builder()
            .vertexShader("shaders/shader.vert")
            .fragmentShader("shaders/shader.frag")
            .build(*engine, *swapChain);

        const auto shaderInstance = shader->createInstance(*engine);

        const auto scene = Scene::create();

        // The render loop
        context->loop([] {});

        // When we exit the loop, drawing and presentation operations may still be going on.
        // Cleaning up resources while that is happening is a bad idea.
        engine->waitIdle();

        // Destroy all rendering resources
        engine->destroyShaderInstance(shaderInstance);
        engine->destroyShader(shader);
        engine->destroyBuffer(indexBuffer);
        engine->destroyBuffer(vertexBuffer);
        engine->destroySwapChain(swapChain);
        engine->destroy();

        // Destroy the window context
        context->destroy();
    } catch (const std::exception& e) {
        PLOGE << e.what();
        return EXIT_FAILURE;
    }

    return 0;
}
