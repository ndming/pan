#pragma once

#include <engine/Overlay.h>

#include <mutex>


class GUI final : public Overlay {
public:
    void define() override;

    void updateCurrentImageCoordinates(int x, int y);
    void clearCurrentImageCoordinates();

    void updateSpectralCurve(const std::vector<float>& values);
    void updateSpectralCurve(std::vector<float>&& values) noexcept;

private:
    void definePerformanceMetricWindow();
    void defineSpectralCurveWindow();
    void defineIlluminantWindow();

    std::mutex _imgCoordinatesMutex{};
    int _currentImgX{ -1 };
    int _currentImgY{ -1 };

    std::mutex _spectralCurveMutex{};
    std::vector<float> _spectralCurve{};
};
