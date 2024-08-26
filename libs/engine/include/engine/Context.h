#pragma once

#include <functional>
#include <memory>
#include <string_view>


struct GLFWwindow;
using Surface = GLFWwindow;

class Context final {
public:
    static std::unique_ptr<Context> create(std::string_view name);
    void destroy() const noexcept;

    static void setOnMouseClick(const std::function<void(double, double)>& callback);
    static void setOnMouseClick(std::function<void(double, double)>&& callback) noexcept;

    [[nodiscard]] Surface* getSurface() const;

    void loop(const std::function<void()>& onFrame) const;

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

private:
    explicit Context(std::string_view name);

    GLFWwindow* _window;
};
