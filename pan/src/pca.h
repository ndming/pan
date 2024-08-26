#pragma once

#include <filesystem>
#include <vector>


namespace pca {
    static constexpr auto MAX_COMPONENTS = 32;
    std::vector<std::vector<float>> readVectors(const std::filesystem::path& path, int bandCount);

    struct PCA {
        alignas(4) int componentCount;
        alignas(4) int maxComponents{ MAX_COMPONENTS };
    };

    struct Vector {
        std::vector<float> data;
    };
}
