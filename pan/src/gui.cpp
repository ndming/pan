#include "gui.h"
#include "pca.h"
#include "spd.h"

#include <imgui.h>

#include <format>
#include <ranges>


void GUI::define() {
    definePerformanceMetricWindow();
    defineSpectralCurveWindow();
    defineIlluminantWindow();
    defineSensorWindow();
    definePCAWindow();
}

void GUI::definePerformanceMetricWindow() {
    ImGui::SetNextWindowDockID(ImGui::GetID("Metric"), ImGuiCond_FirstUseEver);

    ImGui::SetNextWindowPos(ImVec2(1280, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

    ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Performance metrics", nullptr, ImGuiWindowFlags_NoCollapse);

    // Show frame rate (frames per second)
    ImGui::Text("Frame rate: %.1f FPS", io.Framerate);

    // Show frame time (milliseconds per frame)
    ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / io.Framerate);

    ImGui::End();
}

void GUI::defineSpectralCurveWindow() {
    // Ensure the window has a unique name and is docked correctly
    ImGui::SetNextWindowDockID(ImGui::GetID("SpectralCurve"), ImGuiCond_FirstUseEver);

    // Set the window position and size (optional if docking is enabled)
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 300), ImGuiCond_FirstUseEver);

    ImGui::Begin("Spectral reflectance", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    std::lock_guard lock(_spectralCurveMutex);
    if (!_spectralCurve.empty()) {
        std::lock_guard imgCoordLock(_imgCoordinatesMutex);
        ImGui::Text(std::format("Reflectance values at ({}, {})", _currentImgX, _currentImgY).c_str());

        const auto valueCount = static_cast<int>(_spectralCurve.size());
        ImGui::PlotLines("", _spectralCurve.data(), valueCount, 0, nullptr, 0.0f, 1.0f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
    } else {
        ImGui::Text("No data to display.");
    }

    ImGui::End();
}

void GUI::defineIlluminantWindow() {
    // Ensure the window has a unique name and is docked correctly
    ImGui::SetNextWindowDockID(ImGui::GetID("Illuminant"), ImGuiCond_FirstUseEver);

    // Set the window position and size (optional if docking is enabled)
    // ImGui::SetNextWindowPos(ImVec2(0, 300), ImGuiCond_FirstUseEver);
    // ImGui::SetNextWindowSize(ImVec2(600, 800), ImGuiCond_FirstUseEver);

    ImGui::Begin("Current Illuminant", nullptr, ImGuiWindowFlags_NoCollapse);
    static const char* illuminants[] = { "D65", "D50", "A" };
    ImGui::Combo("Select Option", &_currentIlluminant, illuminants, IM_ARRAYSIZE(illuminants));

    switch (getCurrentIlluminant()) {
        case spd::Illuminant::D65:
            ImGui::PlotLines("", spd::D65.data(), spd::D65.size(), 0, nullptr, 0.0f, 300.0f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            break;
        case spd::Illuminant::D50:
            ImGui::PlotLines("", spd::D50.data(), spd::D50.size(), 0, nullptr, 0.0f, 300.0f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            break;
        case spd::Illuminant::A:
            ImGui::PlotLines("", spd::A.data(), spd::A.size(), 0, nullptr, 0.0f, 300.0f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            break;
    }

    ImGui::End();
}

void GUI::defineSensorWindow() {
    // Ensure the window has a unique name and is docked correctly
    ImGui::SetNextWindowDockID(ImGui::GetID("Sensor"), ImGuiCond_FirstUseEver);

    ImGui::Begin("Current Sensor", nullptr, ImGuiWindowFlags_NoCollapse);
    static const char* illuminants[] = { "CIE1931 - 2 degree", "CIE1964 - 10 degree" };
    ImGui::Combo("Select Option", &_currentSensor, illuminants, IM_ARRAYSIZE(illuminants));

    switch (getCurrentSensor()) {
        case spd::Sensor::CIE1931:
            ImGui::PlotLines("", spd::CIE1931_X.data(), spd::CIE1931_X.size(), 0, nullptr, 0.0f, 1.5f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            ImGui::PlotLines("", spd::CIE1931_Y.data(), spd::CIE1931_Y.size(), 0, nullptr, 0.0f, 1.5f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            ImGui::PlotLines("", spd::CIE1931_Z.data(), spd::CIE1931_Z.size(), 0, nullptr, 0.0f, 1.5f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            break;
        case spd::Sensor::CIE1964:
            ImGui::PlotLines("", spd::CIE1964_X.data(), spd::CIE1964_X.size(), 0, nullptr, 0.0f, 1.5f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            ImGui::PlotLines("", spd::CIE1964_Y.data(), spd::CIE1964_Y.size(), 0, nullptr, 0.0f, 1.5f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            ImGui::PlotLines("", spd::CIE1964_Z.data(), spd::CIE1964_Z.size(), 0, nullptr, 0.0f, 1.5f, ImVec2{ PLOT_SIZE_X, PLOT_SIZE_Y });
            break;
    }

    ImGui::End();
}

void GUI::definePCAWindow() {
    ImGui::SetNextWindowDockID(ImGui::GetID("PCA"), ImGuiCond_FirstUseEver);
    ImGui::Begin("Principle Component Analysis", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text("No. of principle components");
    ImGui::SliderInt("##", &_currentComponentCount, 1, 32);

    using namespace std::ranges;
    const auto variability = fold_left(pca::eigenvalues | views::take(_currentComponentCount), 0.0f, std::plus{}) /
        fold_left(pca::eigenvalues, 0.0f, std::plus{});
    ImGui::Text(std::format("Variability: {:.4f}%%", variability * 100.0f).c_str());

    ImGui::End();
}

void GUI::updateCurrentImageCoordinates(const int x, const int y) {
    std::lock_guard lock(_imgCoordinatesMutex);
    _currentImgX = x;
    _currentImgY = y;
}

void GUI::clearCurrentImageCoordinates() {
    std::lock_guard lock(_imgCoordinatesMutex);
    _currentImgX = -1;
    _currentImgY = -1;
}

void GUI::updateSpectralCurve(const std::vector<float>& values) {
    std::lock_guard lock(_spectralCurveMutex);
    _spectralCurve = values;
}

void GUI::updateSpectralCurve(std::vector<float>&& values) noexcept {
    std::lock_guard lock(_spectralCurveMutex);
    _spectralCurve = std::move(values);
}

spd::Illuminant GUI::getCurrentIlluminant() const {
    return static_cast<spd::Illuminant>(_currentIlluminant);
}

spd::Sensor GUI::getCurrentSensor() const {
    return static_cast<spd::Sensor>(_currentSensor);
}

int GUI::getCurrentComponentCount() const {
    return _currentComponentCount;
}
