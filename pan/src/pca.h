#pragma once

#include <array>
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

    static constexpr auto eigenvalues = std::array{
        1.4926708e-01f, 9.8896138e-03f, 4.9601955e-04f, 1.7907383e-04f,
        1.1601748e-04f, 2.9017523e-05f, 2.0653080e-05f, 1.7469722e-05f,
        1.3450323e-05f, 8.1258067e-06f, 7.2624939e-06f, 6.2220984e-06f,
        5.9526533e-06f, 5.0045296e-06f, 4.0007853e-06f, 3.2536450e-06f,
        2.8706961e-06f, 2.2846725e-06f, 2.0022724e-06f, 1.8425326e-06f,
        1.7667120e-06f, 1.6127684e-06f, 1.5368057e-06f, 1.4434830e-06f,
        1.4016589e-06f, 1.3296311e-06f, 1.2624442e-06f, 1.2015678e-06f,
        1.1655262e-06f, 1.0377366e-06f, 1.0006025e-06f, 9.3745689e-07f,
    };
}
