#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include <GLFW/glfw3.h>


class Context {
public:
    static std::unique_ptr<Context> create(std::string_view name, int width = 1280, int height = 768);
    void destroy() const noexcept;

    [[nodiscard]] GLFWwindow* getNativeWindow() const;

    void loop(const std::function<void()>& onFrame) const;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

private:
    Context(std::string_view name, int width, int height);

    GLFWwindow* _window{ nullptr };
};