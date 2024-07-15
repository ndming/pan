cmake_minimum_required(VERSION 3.22)

set(TARGET engine)

set(SRCS
        src/Engine.cpp
        src/Renderer.cpp
        src/ResourceAllocator.cpp
        src/SwapChain.cpp
        src/Translator.cpp
        src/bootstrap/DebugMessenger.cpp
        src/bootstrap/GraphicsPipelineBuilder.cpp
        src/bootstrap/InstanceBuilder.cpp
        src/bootstrap/PhysicalDeviceSelector.cpp
        src/bootstrap/QueueFamilyFinder.cpp
        src/bootstrap/RenderPassBuilder.cpp
        src/bootstrap/SwapChainBuilder.cpp
        src/external/VmaUsage.cpp
)

# Build a library
add_library(${TARGET} STATIC)
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)
target_sources(${TARGET} PRIVATE ${SRCS})
target_compile_definitions(${TARGET} PUBLIC GLFW_INCLUDE_NONE)

# Where to look for header files
target_include_directories(${TARGET} PUBLIC include)
target_include_directories(${TARGET} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(${TARGET} PRIVATE src)

# Dependencies
target_link_libraries(${TARGET} PUBLIC plog::plog)
target_link_libraries(${TARGET} PUBLIC glfw)
target_link_libraries(${TARGET} PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(${TARGET} PRIVATE glm::glm)

# Compile shaders to the build directory
find_program(GLSL_COMPILER glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
file(GLOB_RECURSE GLSL_SOURCE_FILES "shaders/*.frag" "shaders/*.vert" "shaders/*.comp")
foreach(GLSL_FILE ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    message(STATUS "Compiling shader file: ${FILE_NAME}")
    set(SPIR_V "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIR_V}
            COMMAND ${GLSL_COMPILER} -o ${SPIR_V} ${GLSL_FILE}
            DEPENDS ${GLSL_FILE}
            COMMENT "Compiling ${GLSL_FILE} to ${SPIR_V}")
    list(APPEND SPIR_V_BINARY_FILES ${SPIR_V})
endforeach(GLSL_FILE)

# Add a custom target to build all SPIR-V binaries
add_custom_target(compile_shaders ALL DEPENDS ${SPIR_V_BINARY_FILES})

# Ensure the main target depends on the compiled shaders
add_dependencies(${TARGET} compile_shaders)
