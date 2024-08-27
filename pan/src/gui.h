#pragma once

#include "spd.h"

#include <engine/Overlay.h>

#include <mutex>


class GUI final : public Overlay {
public:
    void define() override;

    void updateCurrentImageCoordinates(int x, int y);
    void clearCurrentImageCoordinates();

    void updateSpectralCurve(const std::vector<float>& values);
    void updateSpectralCurve(std::vector<float>&& values) noexcept;

    spd::Illuminant getCurrentIlluminant() const;
    spd::Sensor getCurrentSensor() const;

    int getCurrentComponentCount() const;

private:
    void definePerformanceMetricWindow();
    void defineSpectralCurveWindow();
    void defineIlluminantWindow();
    void defineSensorWindow();
    void definePCAWindow();

    std::mutex _imgCoordinatesMutex{};
    int _currentImgX{ -1 };
    int _currentImgY{ -1 };

    std::mutex _spectralCurveMutex{};
    std::vector<float> _spectralCurve{};

    int _currentIlluminant{ static_cast<int>(spd::Illuminant::D65) };
    int _currentSensor{ static_cast<int>(spd::Sensor::CIE1931) };

    static constexpr auto PLOT_SIZE_X = 580;
    static constexpr auto PLOT_SIZE_Y = 200;

    int _currentComponentCount{ 3 };
};
