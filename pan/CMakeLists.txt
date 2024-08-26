cmake_minimum_required(VERSION 3.22)

find_package(GDAL CONFIG REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)

set(TARGET pan)

set(SRCS
        src/gui.cpp
        src/main.cpp
        src/pan.cpp
        src/pca.cpp
        src/spd.cpp
        src/stb.cpp
)

# Build an executable
add_executable(${TARGET} ${SRCS})
set_target_properties(${TARGET} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF COMPILE_WARNING_AS_ERROR ON)

# Where to look for header files
target_include_directories(${TARGET} PRIVATE src)

# Dependencies
target_link_libraries(${TARGET} PRIVATE engine)
target_link_libraries(${TARGET} PRIVATE GDAL::GDAL)
target_link_libraries(${TARGET} PRIVATE Eigen3::Eigen)

# Shader resources
set(SPIR_V_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SHADER_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/pca.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/xyz.frag
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
)
compile_shaders(${TARGET} ${SPIR_V_OUTPUT_DIR} "${SHADER_FILES}")
target_link_libraries(${TARGET} PRIVATE ${TARGET}_shaders)

# Texture resources
set(TEXTURE_DST_DIR "${CMAKE_CURRENT_BINARY_DIR}/textures")
set(TEXTURE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/textures/statue.jpg
        ${CMAKE_CURRENT_SOURCE_DIR}/textures/image.png
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

# Assets
set(ASSET_DST_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(ASSET_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/img
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/pca.txt
)
foreach(ASSET_FILE ${ASSET_FILES})
    # Get the filename from the full path
    get_filename_component(FILE_NAME ${ASSET_FILE} NAME)
    set(ASSET_DST_FILE "${ASSET_DST_DIR}/${FILE_NAME}")

    # Add a custom command to copy the texture file
    add_custom_command(
            OUTPUT ${ASSET_DST_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy ${ASSET_FILE} ${ASSET_DST_FILE}
            DEPENDS ${ASSET_FILE}
            COMMENT "Copying ${FILE_NAME} to build directory"
    )

    # Collect the list of destination files
    list(APPEND ASSET_DST_FILES ${ASSET_DST_FILE})
endforeach()
add_custom_target(copy_assets ALL DEPENDS ${ASSET_DST_FILES})
add_dependencies(${TARGET} copy_assets)
