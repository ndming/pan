cmake_minimum_required(VERSION 3.22)

set(TARGET pan)

set(SRCS
        src/gui.cpp
        src/main.cpp
        src/stb.cpp
)

# Build an executable
add_executable(${TARGET} ${SRCS})
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)

# Where to look for header files
target_include_directories(${TARGET} PRIVATE src)

# Dependencies
target_link_libraries(${TARGET} PRIVATE engine)

# Shader resources
set(SPIR_V_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SHADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
)
compile_shaders(${TARGET} ${SPIR_V_OUTPUT_DIR} "${SHADER_FILES}")
target_link_libraries(${TARGET} PRIVATE ${TARGET}_shaders)

# Texture resources
set(TEXTURE_DST_DIR "${CMAKE_CURRENT_BINARY_DIR}/textures")
set(TEXTURE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/textures/statue.jpg
)
foreach(TEXTURE_FILE ${TEXTURE_FILES})
    # Get the filename from the full path
    get_filename_component(FILE_NAME ${TEXTURE_FILE} NAME)
    set(TEXTURE_DST_FILE "${TEXTURE_DST_DIR}/${FILE_NAME}")

    # Add a custom command to copy the texture file
    add_custom_command(
            OUTPUT ${TEXTURE_DST_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy ${TEXTURE_FILE} ${TEXTURE_DST_FILE}
            DEPENDS ${TEXTURE_FILE}
            COMMENT "Copying ${FILE_NAME} to build directory"
    )

    # Collect the list of destination files
    list(APPEND TEXTURE_DST_FILES ${TEXTURE_DST_FILE})
endforeach()
add_custom_target(copy_textures ALL DEPENDS ${TEXTURE_DST_FILES})
add_dependencies(${TARGET} copy_textures)
