cmake_minimum_required(VERSION 3.22)

project(pan
    VERSION 1.0.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# External dependencies
find_package(Vulkan REQUIRED)
add_subdirectory(ext)

# Internal dependencies
add_subdirectory(libs/engine)

add_subdirectory(pan)
