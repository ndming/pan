cmake_minimum_required(VERSION 3.22)

set(TARGET engine)

set(SRCS
        src/Buffer.cpp
        src/Composable.cpp
        src/ComputeShader.cpp
        src/Context.cpp
        src/Drawable.cpp
        src/Engine.cpp
        src/GraphicShader.cpp
        src/IndexBuffer.cpp
        src/Overlay.cpp
        src/Renderer.cpp
        src/Scene.cpp
        src/Shader.cpp
        src/ShaderInstance.cpp
        src/SwapChain.cpp
        src/Transformable.cpp
        src/UniformBuffer.cpp
        src/VertexBuffer.cpp
        src/View.cpp
        src/allocator/ResourceAllocator.cpp
        src/allocator/VmaUsage.cpp
        src/bootstrap/DebugMessenger.cpp
        src/bootstrap/DeviceBuilder.cpp
        src/bootstrap/InstanceBuilder.cpp
        src/bootstrap/PhysicalDeviceSelector.cpp
        src/bootstrap/QueueFamilyFinder.cpp
)

# Build a library
add_library(${TARGET} STATIC)
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)
target_sources(${TARGET} PRIVATE ${SRCS})
target_compile_definitions(${TARGET} PUBLIC GLFW_INCLUDE_NONE)
target_compile_definitions(${TARGET} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

# Where to look for header files
target_include_directories(${TARGET} PUBLIC include)
target_include_directories(${TARGET} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(${TARGET} PRIVATE src)

# Dependencies
target_link_libraries(${TARGET} PUBLIC plog::plog)
target_link_libraries(${TARGET} PUBLIC glm::glm)
target_link_libraries(${TARGET} PUBLIC gui)
target_link_libraries(${TARGET} PRIVATE glfw)
target_link_libraries(${TARGET} PRIVATE ${Vulkan_LIBRARIES})

# Find the GLSL compiler
find_program(GLSL_COMPILER glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
message(STATUS "Using GLSL compiler at: ${GLSL_COMPILER}")

# Shader compilation
function(compile_shaders
        TARGET_NAME
        SPIR_V_OUTPUT_DIR
        GLSL_SOURCE_FILES)
    file(MAKE_DIRECTORY ${SPIR_V_OUTPUT_DIR})

    foreach(GLSL_FILE ${GLSL_SOURCE_FILES})
        get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
        set(SPIR_V "${SPIR_V_OUTPUT_DIR}/${FILE_NAME}.spv")

        add_custom_command(
                OUTPUT ${SPIR_V}
                COMMAND ${GLSL_COMPILER} -o ${SPIR_V} ${GLSL_FILE}
                DEPENDS ${GLSL_FILE}
                COMMENT "Compiling ${FILE_NAME}")

        list(APPEND SPIR_V_BINARY_FILES ${SPIR_V})
    endforeach(GLSL_FILE)

    # Create an INTERFACE library to represent the compiled shaders
    add_library(${TARGET_NAME}_shaders INTERFACE)
    target_sources(${TARGET_NAME}_shaders INTERFACE ${SPIR_V_BINARY_FILES})
endfunction()
