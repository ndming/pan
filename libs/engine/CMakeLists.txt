cmake_minimum_required(VERSION 3.22)

find_package(Vulkan REQUIRED)

set(TARGET engine)

set(SRCS
        src/Engine.cpp
)

# Build a library
add_library(${TARGET} STATIC ${SRCS})
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)

# Where to look for header files
target_include_directories(${TARGET} PUBLIC include)
target_include_directories(${TARGET} PRIVATE src)
target_include_directories(${TARGET} PRIVATE ${Vulkan_INCLUDE_DIRS})

# Dependencies
target_link_libraries(${TARGET} PRIVATE ${Vulkan_LIBRARIES})
