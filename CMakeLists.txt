cmake_minimum_required(VERSION 3.20)

project(PingoComposer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect all source files under src/.
# CONFIGURE_DEPENDS makes CMake look for new files when building,
# so new .cpp/.h files do not have to be added by hand.
file(GLOB_RECURSE COMPOSER_SOURCES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/src/*.cpp
        ${PROJECT_SOURCE_DIR}/src/*.h
)

add_executable(PingoComposer ${COMPOSER_SOURCES})

# Includes start at src/, so "Engine/FontRenderer.h" is written the same everywhere
target_include_directories(PingoComposer PRIVATE ${PROJECT_SOURCE_DIR}/src)

find_package(raylib REQUIRED)

target_link_libraries(PingoComposer PRIVATE raylib)

if (NOT MSVC)
    target_compile_options(PingoComposer PRIVATE -Wall -Wextra)
endif ()

# Unlike the game engine there is only one build: the composer is a tool, it
# has no release mode without its editing tools.

# Copy assets into the build directory.
# Not as POST_BUILD, because that only runs when code was recompiled as well:
# a tweaked file would otherwise never reach the program. PingoComposer
# depends on this target instead, so the IDE copies when starting it.
add_custom_target(CopyAssets
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:PingoComposer>/assets
        COMMENT "Kopiere Assets"
)

add_dependencies(PingoComposer CopyAssets)
