#include "CLI11.hpp"
#include "pan.h"
#include "pca.h"
#include "gui.h"
#include "spd.h"
#include "stb.h"

#include <engine/Context.h>
#include <engine/Engine.h>
#include <engine/VertexBuffer.h>
#include <engine/IndexBuffer.h>
#include <engine/UniformBuffer.h>
#include <engine/Texture.h>
#include <engine/GraphicShader.h>
#include <engine/Drawable.h>
#include <engine/View.h>

#include <gdal_priv.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include <ranges>
#include <filesystem>
#include <engine/StorageBuffer.h>


int main(const int argc, char* argv[]) {
    // CLI arguments
    auto pan = CLI::App{};

    auto filePath = std::string{};
    auto downscaleFactor = 4;

    const auto multipleOf2 = [](const std::string& str) {
        const int value = std::stoi(str);
        return value % 2 != 0 ? "Downscaling factor must be a multiple of 2" : std::string{};
    };
    pan.add_option("input", filePath, "A supported image file: ENVI")->required()->check(CLI::ExistingFile);
    pan.add_option("--downscale", downscaleFactor, "Downscaling factor in both axes")
        ->check(CLI::PositiveNumber)
        ->check(multipleOf2);

    try {
        CLI11_PARSE(pan, argc, argv);
    } catch (const CLI::ParseError& e) {
        pan.exit(e);
    }

    // Plant a logger
    auto appender = plog::ColorConsoleAppender<plog::TxtFormatter>();
#ifdef NDEBUG
    init(plog::info, &appender);
#else
    init(plog::debug, &appender);
#endif

    GDALAllRegister();
    const auto pathAbsolute = std::filesystem::absolute(filePath);

    // Open the dataset
    const auto dataset = static_cast<GDALDataset*>(GDALOpen(pathAbsolute.string().c_str(), GA_ReadOnly));
    if (dataset == nullptr) {
        PLOGE << "Failed to open input file.";
        return 1;
    }

    // Peak input dimensions
    const auto imgYSize = dataset->GetRasterYSize();
    const auto imgXSize = dataset->GetRasterXSize();
    PLOGD << "Image rows: " << imgYSize;
    PLOGD << "Image cols: " << imgXSize;
    PLOGD << "Band count: " << dataset->GetRasterCount();

    const auto bufferXSize = imgXSize / downscaleFactor;
    const auto bufferYSize = imgYSize / downscaleFactor;
    PLOGD << "Spatial resolution: " << bufferXSize << " x " << bufferYSize;

    // Get center wavelengths of each band
    const auto metadata = dataset->GetMetadata();
    const auto centerWavelengths = parseMetadata(metadata, CSLCount(metadata))
        | std::views::transform([](const auto it) { return static_cast<uint32_t>(it); })
        | std::ranges::to<std::vector>();

    static constexpr auto MIN_WAVELENGTH = 360;
    static constexpr auto MAX_WAVELENGTH = 830;

    // Find target wavelengths
    auto bandBegin = 0;
    auto bandEnd = static_cast<int>(centerWavelengths.size());
    while (centerWavelengths[bandBegin] < MIN_WAVELENGTH) ++bandBegin;
    while (centerWavelengths[bandEnd - 1] > MAX_WAVELENGTH) --bandEnd;

    if (bandBegin >= bandEnd) {
        PLOGE << "The dataset covers an unsuitable wavelength range";
        GDALClose(dataset);
        return 1;
    }
    PLOGD << "Spectral resolution: " << bandEnd - bandBegin;

    // Create a window context
    const auto context = Context::create("pan");

    // Create an engine
    const auto engine = Engine::create(context->getSurface());

    // Create a swap chain and a renderer
    const auto swapChain = engine->createSwapChain();
    const auto renderer = engine->createRenderer();

    // Create a quad
    static constexpr auto OFFSET_X = -1.0f;
    const auto imgRatio = static_cast<float>(imgXSize) / static_cast<float>(imgYSize);
    static const auto positions = std::array{
        glm::vec3{ -QUAD_SIDE_HALF_EXTENT * imgRatio, -QUAD_SIDE_HALF_EXTENT, 0.0f },
        glm::vec3{ -QUAD_SIDE_HALF_EXTENT * imgRatio,  QUAD_SIDE_HALF_EXTENT, 0.0f },
        glm::vec3{  QUAD_SIDE_HALF_EXTENT * imgRatio, -QUAD_SIDE_HALF_EXTENT, 0.0f },
        glm::vec3{  QUAD_SIDE_HALF_EXTENT * imgRatio,  QUAD_SIDE_HALF_EXTENT, 0.0f },
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
        glm::vec2{ 1.0f, 0.0f },
        glm::vec2{ 1.0f, 1.0f },
    };

    const auto vertexBuffer = VertexBuffer::Builder()
        .vertexCount(4)
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

    static constexpr auto indices = std::array<uint16_t, 6>{ 0, 1, 2, 3 };
    const auto indexBuffer = IndexBuffer::Builder()
        .indexCount(indices.size())
        .indexType(IndexType::Uint16)
        .build(*engine);
    indexBuffer->setData(indices.data(), *engine);

    const auto illuminant = UniformBuffer::Builder()
        .dataByteSize(sizeof(Illuminant))
        .build(*engine);

    const auto sensor = UniformBuffer::Builder()
        .dataByteSize(sizeof(Sensor))
        .build(*engine);

    const auto dimension = UniformBuffer::Builder()
        .dataByteSize(sizeof(Dimension))
        .build(*engine);
    const auto dimensionObject = Dimension{ bufferXSize, bufferYSize, bandEnd - bandBegin };
    dimension->setData(&dimensionObject);

    auto illuminantObject = Illuminant{};
    auto sensorObject = Sensor{};
    int index = 0;
    for (auto b = bandBegin; b < bandEnd; ++b) {
        const auto wavelength = centerWavelengths[b];
        illuminantObject.data[index] = getIlluminantValueAt(wavelength, spd::Illuminant::D65);
        sensorObject.x[index] = getSensorXValueAt(wavelength, spd::Sensor::CIE1931);
        sensorObject.y[index] = getSensorYValueAt(wavelength, spd::Sensor::CIE1931);
        sensorObject.z[index] = getSensorZValueAt(wavelength, spd::Sensor::CIE1931);
        index += 4;
    }
    illuminant->setData(&illuminantObject);
    sensor->setData(&sensorObject);

    const auto rasters = std::views::iota(bandBegin, bandEnd)
        | std::views::transform([&](const int bandIndex) {
            const auto raster = StorageBuffer::Builder()
                .byteSize(sizeof(float) * bufferXSize * bufferYSize)
                .build(*engine);

            auto values = std::vector<float>(imgXSize * imgYSize);
            const auto band = dataset->GetRasterBand(bandIndex + 1);
            [[maybe_unused]] const auto err = band->RasterIO(
                GF_Read, 0, 0, imgXSize, imgYSize, values.data(), bufferXSize, bufferYSize, GDT_Float32, 0, 0);

            raster->setData(values.data(), *engine);

            return raster; })
        | std::ranges::to<std::vector>();

    const auto shader = GraphicShader::Builder()
        .vertexShader("shaders/shader.vert")
        .fragmentShader("shaders/xyz.frag")
        .descriptorCount(4)
        .descriptor(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(3, vk::DescriptorType::eStorageBuffer, 128, vk::ShaderStageFlagBits::eFragment)
        .build(*engine, *swapChain);

    const auto shaderInstance = shader->createInstance(*engine);
    shaderInstance->setDescriptor(0, illuminant, *engine);
    shaderInstance->setDescriptor(1, sensor, *engine);
    shaderInstance->setDescriptor(2, dimension, *engine);
    shaderInstance->setDescriptor(3, rasters, *engine);

    const auto xyzQuad = Drawable::Builder(1)
        .geometry(0, Drawable::Topology::TriangleStrip, vertexBuffer, indexBuffer, indices.size())
        .material(0, shaderInstance)
        .build(*engine);
    xyzQuad->setTransform(translate(glm::mat4{ 1.0f }, { OFFSET_X, 0.0f, 0.0f }));

    const auto pcaShader = GraphicShader::Builder()
        .vertexShader("shaders/shader.vert")
        .fragmentShader("shaders/pca.frag")
        .descriptorCount(6)
        .descriptor(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(3, vk::DescriptorType::eStorageBuffer, 128, vk::ShaderStageFlagBits::eFragment)
        .descriptor(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
        .descriptor(5, vk::DescriptorType::eStorageBuffer, 33, vk::ShaderStageFlagBits::eFragment)
        .build(*engine, *swapChain);

    // Read eigenvectors and the mean vector, convert them to storage buffers
    const auto vectors = pca::readVectors("assets/pca.txt", bandEnd - bandBegin)
        | std::views::transform([&](const std::vector<float>& data) {
            const auto vector = StorageBuffer::Builder()
                .byteSize(sizeof(float) * data.size())
                .build(*engine);
            vector->setData(data.data(), *engine);
            return vector; })
        | std::ranges::to<std::vector>();

    const auto pca = UniformBuffer::Builder()
        .dataByteSize(sizeof(pca::PCA))
        .build(*engine);
    auto pcaObject = pca::PCA{ 3, pca::MAX_COMPONENTS };
    pca->setData(&pcaObject);

    const auto pcaShaderInstance = pcaShader->createInstance(*engine);
    pcaShaderInstance->setDescriptor(0, illuminant, *engine);
    pcaShaderInstance->setDescriptor(1, sensor, *engine);
    pcaShaderInstance->setDescriptor(2, dimension, *engine);
    pcaShaderInstance->setDescriptor(3, rasters, *engine);
    pcaShaderInstance->setDescriptor(4, pca, *engine);
    pcaShaderInstance->setDescriptor(5, vectors, *engine);

    const auto pcaQuad = Drawable::Builder(1)
        .geometry(0, Drawable::Topology::TriangleStrip, vertexBuffer, indexBuffer, indices.size())
        .material(0, pcaShaderInstance)
        .build(*engine);
    pcaQuad->setTransform(translate(glm::mat4{ 1.0f }, { -OFFSET_X, 0.0f, 0.0f }));

    const auto drawShader = GraphicShader::Builder()
        .vertexShader("shaders/draw.vert")
        .fragmentShader("shaders/draw.frag")
        .build(*engine, *swapChain);

    const auto drawShaderInstance = drawShader->createInstance(*engine);

    const auto markVertexBuffer = buildMarkVertexBuffer(*engine);
    const auto markIndexBuffer = buildMarkIndexBuffer(*engine);
    const auto mark = Drawable::Builder(1)
        .geometry(0, Drawable::Topology::TriangleFan, markVertexBuffer, markIndexBuffer, SUBDIVISION_COUNT + 2)
        .material(0, drawShaderInstance)
        .build(*engine);

    const auto frameVertexBuffer = buildFrameVertexBuffer(imgRatio, *engine);
    const auto frameIndexBuffer = buildFrameIndexBuffer(*engine);
    const auto frame = Drawable::Builder(1)
        .geometry(0, Drawable::Topology::LineStrip, frameVertexBuffer, frameIndexBuffer, 5)
        .material(0, drawShaderInstance)
        .build(*engine);

    constexpr auto translateVector = glm::vec3{ -6.0f, 1.5f, 0.0f };
    frame->setTransform(translate(glm::mat4{ 1.0f }, translateVector));

    const auto scene = Scene::create();
    scene->insert(xyzQuad);
    scene->insert(pcaQuad);
    scene->insert(mark);
    scene->insert(frame);

    // Create a camera
    const auto camera = Camera::create();
    camera->setLookAt({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f });
    camera->setProjection(getPanProjection(swapChain->getFramebufferAspectRatio()));

    // Create a view
    const auto view = View::create(*swapChain);
    view->setScene(scene);
    view->setCamera(camera);

    swapChain->setOnFramebufferResize([&] (const uint32_t width, const uint32_t height) {
        camera->setProjection(getPanProjection(swapChain->getFramebufferAspectRatio()));
        view->setViewport(0, 0, width, height);
        view->setScissor(0, 0, width, height);
    });

    Overlay::init(context->getSurface(), *engine, *swapChain);
    const auto gui = std::make_shared<GUI>();

    float quadX{ 0.5f };
    float quadY{ 0.5f };
    mark->setTransform(translate(glm::mat4{ 1.0f },
        glm::vec3{ 0.7f * QUAD_SIDE_HALF_EXTENT * imgRatio * (quadX * 2.0f - 1.0f),
            0.7f * QUAD_SIDE_HALF_EXTENT * (quadY * 2.0f - 1.0f), 0.0f } + translateVector));

    Context::setOnMouseClick([&](const auto x, const auto y) {
        if (getQuadCoordinates(x, y, swapChain->getFramebufferSize(), imgRatio, OFFSET_X, &quadX, &quadY)) {
            // Find image coordinates at this quad location
            const auto imgX = std::min(static_cast<int>(std::round(static_cast<float>(imgXSize) * quadX)), imgXSize - 1);
            const auto imgY = std::min(static_cast<int>(std::round(static_cast<float>(imgYSize) * quadY)), imgYSize - 1);

            gui->updateSpectralCurve(getSpectralValues(dataset, quadX, quadY));
            gui->updateCurrentImageCoordinates(imgX, imgY);
            mark->setTransform(translate(glm::mat4{ 1.0f },
                glm::vec3{ 0.7f * QUAD_SIDE_HALF_EXTENT * imgRatio * (quadX * 2.0f - 1.0f),
                    0.7f * QUAD_SIDE_HALF_EXTENT * (quadY * 2.0f - 1.0f), 0.0f } + translateVector));
        }
    });

    // Set the initial indicator position
    const auto imgX = std::min(static_cast<int>(std::round(static_cast<float>(imgXSize) * 0.5f)), imgXSize - 1);
    const auto imgY = std::min(static_cast<int>(std::round(static_cast<float>(imgYSize) * 0.5f)), imgYSize - 1);
    gui->updateSpectralCurve(getSpectralValues(dataset, 0.5f, 0.5f));
    gui->updateCurrentImageCoordinates(imgX, imgY);

    view->setLineWidth(3.0f);

    // The render loop
    context->loop([&] {
        renderer->render(view, gui, swapChain, [&](const auto frameIndex) {
            // Update current illuminant and sensor
            int i = 0;
            for (auto b = bandBegin; b < bandEnd; ++b) {
                const auto wavelength = centerWavelengths[b];
                illuminantObject.data[i] = getIlluminantValueAt(wavelength, gui->getCurrentIlluminant());
                sensorObject.x[i] = getSensorXValueAt(wavelength, gui->getCurrentSensor());
                sensorObject.y[i] = getSensorYValueAt(wavelength, gui->getCurrentSensor());
                sensorObject.z[i] = getSensorZValueAt(wavelength, gui->getCurrentSensor());
                i += 4;
            }
            illuminant->setData(frameIndex, &illuminantObject);
            sensor->setData(frameIndex, &sensorObject);

            // Update current PCA count
            pcaObject.componentCount = gui->getCurrentComponentCount();
            pca->setData(frameIndex, &pcaObject);
        });
    });

    // When we exit the loop, drawing and presentation operations may still be going on.
    // Cleaning up resources while that is happening is a bad idea.
    engine->waitIdle();

    // Destroy Dear ImGUI components
    Overlay::teardown(*engine);

    // Destroy all rendering resources
    engine->destroyShaderInstance(drawShaderInstance);
    engine->destroyShaderInstance(pcaShaderInstance);
    engine->destroyShaderInstance(shaderInstance);
    engine->destroyShader(drawShader);
    engine->destroyShader(pcaShader);
    engine->destroyShader(shader);
    std::ranges::for_each(vectors, [&engine](const auto it) { engine->destroyBuffer(it); });
    std::ranges::for_each(rasters, [&engine](const auto it) { engine->destroyBuffer(it); });
    engine->destroyBuffer(frameIndexBuffer);
    engine->destroyBuffer(frameVertexBuffer);
    engine->destroyBuffer(markIndexBuffer);
    engine->destroyBuffer(markVertexBuffer);
    engine->destroyBuffer(pca);
    engine->destroyBuffer(dimension);
    engine->destroyBuffer(sensor);
    engine->destroyBuffer(illuminant);
    engine->destroyBuffer(indexBuffer);
    engine->destroyBuffer(vertexBuffer);
    engine->destroyRenderer(renderer);
    engine->destroySwapChain(swapChain);
    engine->destroy();

    // Destroy the window context
    context->destroy();

    // Close the dataset
    GDALClose(dataset);

    return 0;
}
