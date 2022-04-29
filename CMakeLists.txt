cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

# TODO debug/release builds
set(TRACY_ENABLE OFF CACHE STRING "Enable profiling" FORCE)
set(TRACY_ON_DEMAND OFF CACHE STRING "On-demand profiling" FORCE)

add_subdirectory("lib")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.h src/*.cpp
    lib/ImGuiFileDialog/ImGuiFileDialog.cpp
    lib/implot/implot.h lib/implot/implot.cpp lib/implot/implot_items.cpp lib/implot/implot_internal.h
    lib/imgui_club/imgui_memory_editor/imgui_memory_editor.h
    lib/implot/implot_demo.cpp
    )

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(lib/faust/architecture)
include_directories(lib/faust/compiler)
include_directories(lib/concurrentqueue)
include_directories(lib/zep/include)
include_directories(lib/diff-match-patch-cpp-stl)
include_directories(lib/ImGuiFileDialog)
include_directories(lib/implot)
include_directories(lib/imgui_club/imgui_memory_editor)

# `dynamiclib` is faust. TODO export it with a 'faust' name
target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI libsoundio_shared nlohmann_json::nlohmann_json dynamiclib Zep::Zep Tracy::TracyClient)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

add_definitions(-DZEP_USE_SDL) # Needed for ctrl key to work with Zep
