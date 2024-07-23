#pragma once

#include <functional>
#include <memory>
#include <string_view>


struct GLFWwindow;

class Context {
public:
    static std::unique_ptr<Context> create(std::string_view name, int width = 1280, int height = 768);
    ~Context();

    void loop(const std::function<void()>& onFrame) const;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

private:
    Context(std::string_view name, int width, int height);

    GLFWwindow* _window{ nullptr };

    // The Engine needs access to the _window member when creating a SwapChain
    friend class Engine;
};
