#pragma once


class Renderer {
public:
    static constexpr int getMaxFramesInFlight() {
        return MAX_FRAMES_IN_FLIGHT;
    }

private:
    static constexpr auto MAX_FRAMES_IN_FLIGHT = 2;
};
