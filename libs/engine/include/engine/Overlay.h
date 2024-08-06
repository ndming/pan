#pragma once

#include "engine/Context.h"


class Engine;
class SwapChain;

class Overlay {
public:
    static void init(Surface* surface, const Engine& engine, const SwapChain& swapChain);
    static void setMinImageCount(uint32_t minImageCount);
    static void teardown(const Engine& engine);

    virtual void define() = 0;
    virtual ~Overlay() = default;
};
