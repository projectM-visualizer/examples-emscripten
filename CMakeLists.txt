cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

project(projectM_Emscripten)

message(STATUS "${CMAKE_PREFIX_PATH}")

find_package(libprojectM REQUIRED CONFIG)

# Setting the suffix to .html also creates an HTML file in addition to the .js and .wasm outputs.
set(CMAKE_EXECUTABLE_SUFFIX ".html")

add_executable(projectM_SDL_emscripten
        projectM_SDL_emscripten.cpp
        )

target_link_libraries(projectM_SDL_emscripten
        PRIVATE
        libprojectM::static
        )

target_compile_options(projectM_SDL_emscripten
        PRIVATE
        "SHELL:-s USE_SDL=2"
        )

target_link_options(projectM_SDL_emscripten
        PRIVATE
        "SHELL:-s USE_SDL=2"
        "SHELL:-s MIN_WEBGL_VERSION=2"
        "SHELL:-s MAX_WEBGL_VERSION=2"
        "SHELL:-s FULL_ES2=1"
        "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        "SHELL:--preload-file ${PRESETS_DIR}@/presets"
        )
