cmake_minimum_required(VERSION 3.22)

set(TARGET engine)

set(SRCS
        src/Context.cpp
        src/Engine.cpp
        src/SwapChain.cpp
        src/bootstrap/DebugMessenger.cpp
        src/bootstrap/InstanceBuilder.cpp
        src/bootstrap/PhysicalDeviceSelector.cpp
        src/bootstrap/QueueFamilyFinder.cpp
        src/bootstrap/Translator.cpp
        src/external/VmaUsage.cpp
)

# Build a library
add_library(${TARGET} STATIC)
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)
target_sources(${TARGET} PRIVATE ${SRCS})

# Where to look for header files
target_include_directories(${TARGET} PUBLIC include)
target_include_directories(${TARGET} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(${TARGET} PRIVATE src)

# Dependencies
target_link_libraries(${TARGET} PUBLIC plog::plog)
target_link_libraries(${TARGET} PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(${TARGET} PRIVATE glfw)
target_link_libraries(${TARGET} PRIVATE glm::glm)
target_link_libraries(${TARGET} PRIVATE gui)
