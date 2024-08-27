#pragma once

#include <engine/Engine.h>
#include <engine/VertexBuffer.h>
#include <engine/IndexBuffer.h>

#include <glm/glm.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <vector>


static constexpr auto QUAD_SIDE_HALF_EXTENT = 5.0f;
static constexpr auto QUAD_EDGE_PADDING = 0.5f;

glm::mat4 getPanProjection(float framebufferAspectRatio);
bool getQuadCoordinates(float x, float y, const std::pair<int, int>& framebufferSize, float quadAspectRatio, float offsetX, float* quadX, float* quadY);

struct Illuminant {
    alignas(16) std::array<float, 512> data{};
};

struct Sensor {
    alignas(16) std::array<float, 512> x{};
    alignas(16) std::array<float, 512> y{};
    alignas(16) std::array<float, 512> z{};
};

struct Dimension {
    alignas(4) int rasterX;
    alignas(4) int rasterY;
    alignas(4) int rasterCount;
};

struct Raster {
    std::vector<float> data;
};

enum class Region {
    VisiblePurple,
    VisibleViolet,
    VisibleBlue,
    VisibleCyan,
    VisibleGreen,
    VisibleYellow,
    VisibleOrange,
    VisibleRed,
    NearInfrared,
    ShortwaveInfrared,
};

Region getRegion(double wavelengthNano);
std::ostream& operator<<(std::ostream& os, Region region);

// ENVI files
std::vector<std::string> readHeaderFile(const std::filesystem::path& path);
std::vector<double> parseMetadata(char** metadata, int count);

static constexpr auto SUBDIVISION_COUNT = 64;

[[nodiscard]] VertexBuffer* buildMarkVertexBuffer(const Engine& engine);
[[nodiscard]] IndexBuffer* buildMarkIndexBuffer(const Engine& engine);
