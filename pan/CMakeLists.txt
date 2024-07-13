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
