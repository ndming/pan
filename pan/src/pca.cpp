#include "pca.h"

#include <fstream>
#include <iostream>


std::vector<std::vector<float>> pca::readVectors(const std::filesystem::path& path, const int bandCount) {
    auto file = std::ifstream(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    auto vectors = std::vector(MAX_COMPONENTS + 1, std::vector<float>(bandCount));

    auto l = 0;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);

        // Read each value in the line
        auto v = 0;
        float value;
        while (iss >> value) {
            vectors[l][v] = value;
            ++v;
        }

        ++l;
    }

    file.close();
    return vectors;
}
