cmake_minimum_required(VERSION 3.22)

set(TARGET engine)

set(SRCS
        src/Context.cpp
        src/Engine.cpp
        src/Renderer.cpp
        src/ResourceAllocator.cpp
        src/SwapChain.cpp
        src/Translator.cpp
        src/bootstrap/DebugMessenger.cpp
        src/bootstrap/DeviceBuilder.cpp
        src/bootstrap/InstanceBuilder.cpp
        src/bootstrap/PhysicalDeviceSelector.cpp
        src/bootstrap/QueueFamilyFinder.cpp
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
target_link_libraries(${TARGET} PRIVATE glfw)
target_link_libraries(${TARGET} PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(${TARGET} PRIVATE glm::glm)

# Find the GLSL compiler
find_program(GLSL_COMPILER glslc HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
message(STATUS "Using GLSL compiler at: ${GLSL_COMPILER}")

# Define the output directory for SPIR-V binaries
set(SPIR_V_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SPIR_V_OUTPUT_DIR})

set(GLSL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/particle.comp
)
foreach(GLSL_FILE ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL_FILE} NAME)
    set(SPIR_V "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIR_V}
            COMMAND ${GLSL_COMPILER} -o ${SPIR_V} ${GLSL_FILE}
            DEPENDS ${GLSL_FILE}
            COMMENT "Compiling ${FILE_NAME}")
    list(APPEND SPIR_V_BINARY_FILES ${SPIR_V})
endforeach(GLSL_FILE)

# Add a custom target to build all SPIR-V binaries
add_custom_target(compile_shaders ALL DEPENDS ${SPIR_V_BINARY_FILES})
add_dependencies(${TARGET} compile_shaders)
