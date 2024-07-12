#pragma once

#include <memory>
#include <string_view>


struct GLFWwindow;

class Context {
public:
    static std::unique_ptr<Context> create(std::string_view name, int width = 1280, int height = 768);
    void destroy() const;

    [[nodiscard]] void* getNativeWindow() const;

private:
    Context(std::string_view name, int width, int height);

    GLFWwindow* _nativeWindow{ nullptr };
};
