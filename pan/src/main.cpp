#include "gui.h"
#include "stb.h"

#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <engine/Engine.h>
#include <engine/GraphicShader.h>
#include <engine/IndexBuffer.h>
#include <engine/VertexBuffer.h>
#include <engine/Drawable.h>
#include <engine/View.h>
#include <engine/Overlay.h>
#include <engine/Texture.h>

#include <glm/glm.hpp>


struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
};

static constexpr auto positions = std::array{
    glm::vec3{ -0.5f, -0.5f, 0.0f },
    glm::vec3{ -0.5f,  0.5f, 0.0f },
    glm::vec3{  0.5f,  0.5f, 0.0f },
    glm::vec3{  0.5f, -0.5f, 0.0f },
};

static constexpr auto colors = std::array{
    glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f },
    glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f },
    glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f },
    glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f },
};

static constexpr auto texCoords = std::array{
    glm::vec2{ 0.0f, 0.0f },
    glm::vec2{ 0.0f, 1.0f },
    glm::vec2{ 1.0f, 1.0f },
    glm::vec2{ 1.0f, 0.0f },
};

static constexpr auto indices = std::array<uint16_t, 6>{ 0, 1, 2, 0, 2, 3 };

int main(int argc, char* argv[]) {
    // Plant a console logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
    init(plog::verbose, &appender);

    try {
        // Create a window context
        const auto context = Context::create("pan");

        // Create an engine
        const auto engine = Engine::create(context->getSurface(), {
            .samplerAnisotropy = true,
        });

        // Create a swap chain and a renderer
        const auto swapChain = engine->createSwapChain();
        const auto renderer = engine->createRenderer();

        const auto vertexBuffer = VertexBuffer::Builder()
            .vertexCount(positions.size())
            .bindingCount(3)
            .binding(0, sizeof(glm::vec3))
            .binding(1, sizeof(glm::vec4))
            .binding(2, sizeof(glm::vec2))
            .attribute(0, 0, AttributeFormat::Float3)
            .attribute(1, 1, AttributeFormat::Float4)
            .attribute(2, 2, AttributeFormat::Float2)
            .build(*engine);
        vertexBuffer->setData(0, positions.data(), *engine);
        vertexBuffer->setData(1, colors.data(), *engine);
        vertexBuffer->setData(2, texCoords.data(), *engine);

        const auto indexBuffer = IndexBuffer::Builder()
            .indexCount(indices.size())
            .indexType(IndexType::Uint16)
            .build(*engine);
        indexBuffer->setData(indices.data(), *engine);

        int texWidth, texHeight, texChannels;
        const auto pixels = stbi_load("textures/statue.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        const auto texture = Texture::Builder()
            .width(texWidth)
            .height(texHeight)
            .format(Texture::Format::R8G8B8A8_sRGB)
            .shaderStages({ Shader::Stage::Fragment })
            .build(*engine);
        texture->setData(pixels, *engine);
        stbi_image_free(pixels);

        const auto sampler = Sampler::Builder()
            .anisotropyEnabled(true)
            .maxAnisotropy(16.0f)
            .build(*engine);

        const auto shader = GraphicShader::Builder()
            .vertexShader("shaders/shader.vert")
            .fragmentShader("shaders/shader.frag")
            .descriptorCount(1)
            .descriptor(0, DescriptorType::CombinedImageSampler, 1, Shader::Stage::Fragment)
            .build(*engine, *swapChain);

        const auto shaderInstance = shader->createInstance(*engine);
        shaderInstance->setDescriptor(0, texture, sampler, *engine);

        const auto triangle = Drawable::Builder(1)
            .geometry(0, Drawable::Topology::TriangleList, vertexBuffer, indexBuffer, indices.size())
            .material(0, shaderInstance)
            .build(*engine);

        // Create a scene
        const auto scene = Scene::create();
        scene->insert(triangle);

        // Create a camera
        const auto camera = Camera::create();
        camera->setLookAt({ 2.0f, 2.0f, -2.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f });
        camera->setProjection(glm::radians(45.0f), swapChain->getAspectRatio(), 0.1f, 10.0f);

        // Create a view
        const auto view = View::create(*swapChain);
        view->setScene(scene);
        view->setCamera(camera);

        // Init the GUI overlay
        Overlay::init(context->getSurface(), *engine, *swapChain);

        swapChain->setOnFramebufferResize([&view, &swapChain, &camera] (const auto width, const auto height) {
            camera->setProjection(glm::radians(45.0f), swapChain->getAspectRatio(), 0.1f, 10.0f);
            view->setViewport(0, 0, width, height); view->setScissor(0, 0, width, height);
            Overlay::setMinImageCount(swapChain->getMinImageCount());
        });

        const auto gui = std::make_shared<GUI>();

        // The render loop
        context->loop([&] {
            renderer->render(view, gui, swapChain);
        });

        // When we exit the loop, drawing and presentation operations may still be going on.
        // Cleaning up resources while that is happening is a bad idea.
        engine->waitIdle();

        // Destroy the GUI component
        Overlay::teardown(*engine);

        // Destroy all rendering resources
        engine->destroyShaderInstance(shaderInstance);
        engine->destroyShader(shader);
        engine->destroyImage(texture);
        engine->destroySampler(sampler);
        engine->destroyBuffer(indexBuffer);
        engine->destroyBuffer(vertexBuffer);
        engine->destroyRenderer(renderer);
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
