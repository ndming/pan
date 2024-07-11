cmake_minimum_required(VERSION 3.22)

set(TARGET engine)

set(SRCS
        src/Engine.cpp
)

set(INTERNAL_SRCS
        src/bootstrap/DebugMessenger.cpp
        src/bootstrap/InstanceBuilder.cpp
        src/bootstrap/PhysicalDeviceSelector.cpp
        src/bootstrap/QueueFamilyFinder.cpp
        src/external/VmaUsage.cpp
)

# Build a library
add_library(${TARGET} STATIC)
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)
target_sources(${TARGET} PUBLIC ${SRCS})
target_sources(${TARGET} PRIVATE ${INTERNAL_SRCS})

# Where to look for header files
target_include_directories(${TARGET} PUBLIC include)
target_include_directories(${TARGET} PRIVATE src)

if (MSVC)
    target_include_directories(${TARGET} PUBLIC ${Vulkan_INCLUDE_DIRS})
endif ()

# Dependencies
target_link_libraries(${TARGET} PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(${TARGET} PRIVATE glm::glm)
target_link_libraries(${TARGET} PRIVATE gui)
target_link_libraries(${TARGET} PUBLIC glfw)
target_link_libraries(${TARGET} PUBLIC plog::plog)
