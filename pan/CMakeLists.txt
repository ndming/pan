cmake_minimum_required(VERSION 3.22)

set(TARGET pan)

set(SRCS
        src/Context.cpp
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
file(MAKE_DIRECTORY ${SPIR_V_OUTPUT_DIR})
add_dependencies(${TARGET} compile_shaders)
add_custom_command(
        TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_BINARY_DIR}/libs/engine/shaders
        ${SPIR_V_OUTPUT_DIR}
)
