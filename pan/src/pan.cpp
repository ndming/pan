#include "pan.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <fstream>
#include <map>
#include <numbers>
#include <ranges>


glm::mat4 getPanProjection(const float framebufferAspectRatio) {
    static constexpr auto sideLength = QUAD_SIDE_HALF_EXTENT + QUAD_EDGE_PADDING;
    auto proj = glm::ortho(-sideLength * framebufferAspectRatio, sideLength * framebufferAspectRatio, -sideLength, sideLength, 0.1f, 10.0f);
    proj[1][1] *= -1;
    return proj;
}

bool getQuadCoordinates(
    const float x, const float y,
    const std::pair<int, int>& framebufferSize,
    const float quadAspectRatio,
    const float offsetX,
    float* quadX, float* quadY,
    float* posX, float* posY
) {
    const auto frameW = static_cast<float>(framebufferSize.first);
    const auto frameH = static_cast<float>(framebufferSize.second);

    // Transform x, y (screen coordinates) to world space
    const auto scaleFactor = (QUAD_SIDE_HALF_EXTENT + QUAD_EDGE_PADDING) * 2.0f / frameH;
    const auto pX = (x - frameW / 2.0f) * scaleFactor;
    const auto pY = (y - frameH / 2.0f) * scaleFactor;

    if (pX >  QUAD_SIDE_HALF_EXTENT * quadAspectRatio + offsetX ||
        pX < -QUAD_SIDE_HALF_EXTENT * quadAspectRatio + offsetX ||
        std::abs(pY) > QUAD_SIDE_HALF_EXTENT) {
        return false;
    }

    *quadX = (pX + QUAD_SIDE_HALF_EXTENT * quadAspectRatio - offsetX) / (QUAD_SIDE_HALF_EXTENT * quadAspectRatio * 2.0f);
    *quadY = (pY + QUAD_SIDE_HALF_EXTENT) / (QUAD_SIDE_HALF_EXTENT * 2.0f);
    if (posX) *posX = pX;
    if (posY) *posY = pY;

    return true;
}

std::string trim(const std::string& str) {
    const auto strBegin = str.find_first_not_of(" \t\n\r\f\v");
    const auto strEnd = str.find_last_not_of(" \t\n\r\f\v");
    const auto strRange = strEnd - strBegin + 1;
    return strBegin == std::string::npos ? "" : str.substr(strBegin, strRange);
}

std::vector<std::string> readHeaderFile(const std::filesystem::path& path) {
    auto file = std::ifstream{ path };
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    auto keys = std::vector<std::string>{};

    std::string line;
    while (std::getline(file, line)) {
        if (const auto pos = line.find('='); pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            keys.push_back(trim(key));
        }
    }

    file.close();
    return keys;
}

std::vector<double> parseMetadata(char** const metadata, const int count) {
    auto centers = std::vector<double>{};
    for (int i = 0; i < count; ++i) {
        const auto value = std::string{ metadata[i] };

        char* end{};
        if (value.contains("Band")) {
            const auto posEqual = value.find('=');
            const auto centerStr = value.substr(posEqual + 1, value.find(" Nanometers") - posEqual - 1);
            centers.push_back(std::strtod(centerStr.c_str(), &end));
        }
    }

    return centers;
}

std::vector<float> getSpectralValues(GDALDataset* dataset, const float quadX, const float quadY) {
    const auto imgYSize = dataset->GetRasterYSize();
    const auto imgXSize = dataset->GetRasterXSize();
    const auto imgX = std::min(static_cast<int>(std::round(static_cast<float>(imgXSize) * quadX)), imgXSize - 1);
    const auto imgY = std::min(static_cast<int>(std::round(static_cast<float>(imgYSize) * quadY)), imgYSize - 1);

    const auto bandCount = dataset->GetRasterCount();
    auto values = std::vector<float>(bandCount);
    for (int i = 1; i <= bandCount; ++i) {
        const auto band = dataset->GetRasterBand(i);

        float pixelValue = 0.0f;
        [[maybe_unused]] const auto err = band->RasterIO(GF_Read, imgX, imgY, 1, 1, &pixelValue, 1, 1, GDT_Float32, 0, 0);

        values[i - 1] = pixelValue;
    }
    return values;
}

struct RegionInfo {
    uint32_t lower;
    uint32_t upper;
};

static const auto REGION_MAP = std::map<Region, RegionInfo>{
    { Region::VisibleViolet,     { 375,  450  } },
    { Region::VisibleBlue,       { 450,  485  } },
    { Region::VisibleCyan,       { 485,  500  } },
    { Region::VisibleGreen,      { 500,  565  } },
    { Region::VisibleYellow,     { 565,  590  } },
    { Region::VisibleOrange,     { 590,  625  } },
    { Region::VisibleRed,        { 625,  740  } },
    { Region::NearInfrared,      { 740,  1100 } },
    { Region::ShortwaveInfrared, { 1100, 2600 } },
};

Region getRegion(const double wavelengthNano) {
    const auto hasEnclosedRegion = [&wavelengthNano](const auto& region) {
        return  region.second.lower <= wavelengthNano && wavelengthNano < region.second.upper;
    };
    if (const auto found = std::ranges::find_if(REGION_MAP, hasEnclosedRegion); found != REGION_MAP.end()) {
        return found->first;
    }
    throw std::runtime_error("Wavelength is out of range");
}

std::string to_string(const Region region) {
    switch (region) {
        case Region::VisiblePurple:     return "Visible Purple";
        case Region::VisibleViolet:     return "Visible Violet";
        case Region::VisibleBlue:       return "Visible Blue";
        case Region::VisibleCyan:       return "Visible Cyan";
        case Region::VisibleGreen:      return "Visible Green";
        case Region::VisibleYellow:     return "Visible Yellow";
        case Region::VisibleOrange:     return "Visible Orange";
        case Region::VisibleRed:        return "Visible Red";
        case Region::NearInfrared:      return "Near Infrared";
        case Region::ShortwaveInfrared: return "Shortwave Infrared";
        default: return "Unknown Region";
    }
}

std::ostream& operator<<(std::ostream& os, const Region region) {
    return os << to_string(region);
}

glm::vec4 getColor(const int index) {
    switch (index / 8) {
        case 0: return glm::vec4{ 0.7f, 0.0f, 0.0f, 1.0f };
        case 1: return glm::vec4{ 0.7f, 0.3f, 0.0f, 1.0f };
        case 2: return glm::vec4{ 0.8f, 0.7f, 0.0f, 1.0f };
        case 3: return glm::vec4{ 0.0f, 0.8f, 0.4f, 1.0f };
        case 4: return glm::vec4{ 0.0f, 0.8f, 0.8f, 1.0f };
        case 5: return glm::vec4{ 0.0f, 0.1f, 0.5f, 1.0f };
        case 6: return glm::vec4{ 0.3f, 0.0f, 0.5f, 1.0f };
        case 7: return glm::vec4{ 0.5f, 0.0f, 0.5f, 1.0f };
        default: return glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
    }
}

VertexBuffer* buildMarkVertexBuffer(const Engine& engine) {
    auto positions = std::array<glm::vec3, SUBDIVISION_COUNT + 1>{};
    auto colors = std::array<glm::vec4, SUBDIVISION_COUNT + 1>{};

    positions[0] = glm::vec3{ 0.0f, 0.0f, 0.0f };
    colors[0] = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };

    constexpr auto step = 2.0f * std::numbers::pi_v<float> / SUBDIVISION_COUNT;
    for (int i = 0; i < SUBDIVISION_COUNT; ++i) {
        const auto angle = i * step;
        positions[i + 1] = glm::vec3{ 0.08f * glm::cos(-angle), 0.08f * glm::sin(-angle), -1.0f };
        colors[i + 1] = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
    }

    const auto buffer = VertexBuffer::Builder()
        .vertexCount(SUBDIVISION_COUNT + 1)
        .bindingCount(2)
        .binding(0, sizeof(glm::vec3))
        .binding(1, sizeof(glm::vec4))
        .attribute(0, 0, AttributeFormat::Float3)
        .attribute(1, 1, AttributeFormat::Float4)
        .build(engine);
    buffer->setData(0, positions.data(), engine);
    buffer->setData(1, colors.data(), engine);

    return buffer;
}

IndexBuffer* buildMarkIndexBuffer(const Engine& engine) {
    auto indices = std::array<uint16_t, SUBDIVISION_COUNT + 2>{};
    std::ranges::generate_n(indices.begin(), SUBDIVISION_COUNT + 2, [n{ 0 }]() mutable { return n++; });
    indices[SUBDIVISION_COUNT + 1] = 1;

    const auto buffer = IndexBuffer::Builder()
        .indexCount(SUBDIVISION_COUNT + 2)
        .indexType(IndexType::Uint16)
        .build(engine);
    buffer->setData(indices.data(), engine);

    return buffer;
}

VertexBuffer* buildFrameVertexBuffer(const float imgRatio, const Engine& engine) {
    static constexpr auto scale = 0.7f;
    const auto positions = std::array{
        glm::vec3{ -QUAD_SIDE_HALF_EXTENT * imgRatio * scale, -QUAD_SIDE_HALF_EXTENT * scale, 0.0f },
        glm::vec3{ -QUAD_SIDE_HALF_EXTENT * imgRatio * scale,  QUAD_SIDE_HALF_EXTENT * scale, 0.0f },
        glm::vec3{  QUAD_SIDE_HALF_EXTENT * imgRatio * scale, -QUAD_SIDE_HALF_EXTENT * scale, 0.0f },
        glm::vec3{  QUAD_SIDE_HALF_EXTENT * imgRatio * scale,  QUAD_SIDE_HALF_EXTENT * scale, 0.0f },
    };
    static constexpr auto colors = std::array{
        glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f },
        glm::vec4{ 0.0f, 1.0f, 0.0f, 1.0f },
        glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f },
        glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f },
    };

    const auto buffer = VertexBuffer::Builder()
        .vertexCount(4)
        .bindingCount(2)
        .binding(0, sizeof(glm::vec3))
        .binding(1, sizeof(glm::vec4))
        .attribute(0, 0, AttributeFormat::Float3)
        .attribute(1, 1, AttributeFormat::Float4)
        .build(engine);
    buffer->setData(0, positions.data(), engine);
    buffer->setData(1, colors.data(), engine);

    return buffer;
}

IndexBuffer* buildFrameIndexBuffer(const Engine& engine) {
    static constexpr auto indices = std::array<uint16_t, 6>{ 0, 1, 3, 2, 0 };

    const auto buffer = IndexBuffer::Builder()
        .indexCount(SUBDIVISION_COUNT + 1)
        .indexType(IndexType::Uint16)
        .build(engine);
    buffer->setData(indices.data(), engine);

    return buffer;
}
