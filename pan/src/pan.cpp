#include "pan.h"

#include <plog/Log.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <fstream>
#include <map>
#include <ranges>


glm::mat4 getPanProjection(const float framebufferAspectRatio) {
    static constexpr auto sideLength = QUAD_SIDE_HALF_EXTENT + QUAD_EDGE_PADDING;
    return glm::ortho(-sideLength * framebufferAspectRatio, sideLength * framebufferAspectRatio, sideLength, -sideLength, 0.1f, 10.0f);
}

bool getQuadCoordinates(
    const float x, const float y,
    const std::pair<int, int>& framebufferSize,
    const float quadAspectRatio,
    const float offsetX,
    float* quadX, float* quadY
) {
    const auto frameW = static_cast<float>(framebufferSize.first);
    const auto frameH = static_cast<float>(framebufferSize.second);

    // Transform x, y (screen coordinates) to world space
    const auto scaleFactor = (QUAD_SIDE_HALF_EXTENT + QUAD_EDGE_PADDING) * 2.0f / frameH;
    const auto posX = (x - frameW / 2.0f) * scaleFactor;
    const auto posY = (y - frameH / 2.0f) * scaleFactor;

    if (posX >  QUAD_SIDE_HALF_EXTENT * quadAspectRatio + offsetX ||
        posX < -QUAD_SIDE_HALF_EXTENT * quadAspectRatio + offsetX ||
        std::abs(posY) > QUAD_SIDE_HALF_EXTENT) {
        return false;
    }

    *quadX = (posX + QUAD_SIDE_HALF_EXTENT * quadAspectRatio - offsetX) / (QUAD_SIDE_HALF_EXTENT * quadAspectRatio * 2.0f);
    *quadY = (posY + QUAD_SIDE_HALF_EXTENT) / (QUAD_SIDE_HALF_EXTENT * 2.0f);
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
