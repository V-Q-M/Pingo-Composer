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

# On macOS some parts talk to Cocoa directly, e.g. the save panel of the
# system. They live in .mm files: Objective-C++ with automatic reference
# counting.
if (APPLE)
    enable_language(OBJCXX)

    file(GLOB_RECURSE COMPOSER_APPLE_SOURCES CONFIGURE_DEPENDS
            ${PROJECT_SOURCE_DIR}/src/*.mm
    )

    target_sources(PingoComposer PRIVATE ${COMPOSER_APPLE_SOURCES})
    set_source_files_properties(${COMPOSER_APPLE_SOURCES} PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    # AppKit for the panel, UniformTypeIdentifiers for the file endings it offers
    target_link_libraries(PingoComposer PRIVATE "-framework AppKit" "-framework UniformTypeIdentifiers")
endif ()

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
