cmake_minimum_required(VERSION 3.22)

set(TARGET pan)

set(SRCS
        src/main.cpp
)

# Build an executable
add_executable(${TARGET} ${SRCS})
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)

# Where to look for header files
target_include_directories(${TARGET} PRIVATE src)

# Dependencies
target_link_libraries(${TARGET} PRIVATE engine)
target_link_libraries(${TARGET} PRIVATE gui)

# Shader resources
set(SPIR_V_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SHADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
)

compile_shaders(${TARGET} ${SPIR_V_OUTPUT_DIR} "${SHADER_FILES}")
target_link_libraries(${TARGET} PRIVATE ${TARGET}_shaders)
