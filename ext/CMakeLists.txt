cmake_minimum_required(VERSION 3.22)

# Dependencies with CMake support
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(plog)

# Dear ImGUI
set(GUI_SRCS
        gui/imgui.cpp
        gui/imgui_demo.cpp
        gui/imgui_draw.cpp
        gui/imgui_tables.cpp
        gui/imgui_widgets.cpp
        gui/backends/imgui_impl_vulkan.cpp
        gui/backends/imgui_impl_glfw.cpp
        gui/misc/cpp/imgui_stdlib.cpp
)
add_library(gui STATIC)
target_sources(gui PRIVATE ${GUI_SRCS})
target_include_directories(gui PUBLIC gui)
target_include_directories(gui PUBLIC gui/backends)
target_include_directories(gui PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(gui PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(gui PRIVATE glfw)