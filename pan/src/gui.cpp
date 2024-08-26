#include "gui.h"
#include "spd.h"

#include <imgui.h>

#include <format>


void GUI::define() {
    definePerformanceMetricWindow();
    defineSpectralCurveWindow();
    // defineIlluminantWindow();
}

void GUI::definePerformanceMetricWindow() {
    ImGui::SetNextWindowDockID(ImGui::GetID("Metric"), ImGuiCond_FirstUseEver);

    ImGui::SetNextWindowPos(ImVec2(1280, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);

    ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Performance metrics");

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
        constexpr auto plotSize = ImVec2{ 580, 200 };
        ImGui::PlotLines("", _spectralCurve.data(), valueCount, 0, nullptr, 0.0f, 1.0f, plotSize);
    } else {
        ImGui::Text("No data to display.");
    }

    ImGui::End();
}

void GUI::defineIlluminantWindow() {
    // Ensure the window has a unique name and is docked correctly
    ImGui::SetNextWindowDockID(ImGui::GetID("Illuminant"), ImGuiCond_FirstUseEver);

    // Set the window position and size (optional if docking is enabled)
    ImGui::SetNextWindowPos(ImVec2(0, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 800), ImGuiCond_FirstUseEver);

    ImGui::Begin("Illuminants", nullptr, ImGuiWindowFlags_NoMove| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    constexpr auto plotSize = ImVec2{ 580, 200 };

    ImGui::Text("D65");
    ImGui::PlotLines("", spd::D65.data(), spd::D65.size(), 0, nullptr, 0.0f, 280.0f, plotSize);

    ImGui::Text("D50");
    ImGui::PlotLines("", spd::D50.data(), spd::D50.size(), 0, nullptr, 0.0f, 280.0f, plotSize);

    ImGui::Text("A");
    ImGui::PlotLines("", spd::A.data(), spd::A.size(), 0, nullptr, 0.0f, 280.0f, plotSize);

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
