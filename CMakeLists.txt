cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

# TODO debug/release builds
set(TRACY_ENABLE OFF CACHE STRING "Enable profiling" FORCE)
set(TRACY_ON_DEMAND OFF CACHE STRING "On-demand profiling" FORCE)

add_subdirectory("lib")

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp
    lib/ImGuiFileDialog/ImGuiFileDialog.cpp
    lib/implot/implot.cpp lib/implot/implot_items.cpp lib/implot/implot_demo.cpp
    lib/fmt/src/format.cc
    lib/r8brain/r8bbase.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(
    lib/miniaudio
    lib/faust/architecture
    lib/zep/include
    lib/ImGuiFileDialog
    lib/implot
    lib/imgui_club/imgui_memory_editor
    lib/fmt/include
    lib/range-v3/include
    lib/r8brain
    lib/immer
    lib/tracy/public/tracy
    lib/concurrentqueue
)

# See readme for plans with these libraries
#include_directories(lib/diff-match-patch-cpp-stl)

# `dynamiclib` is faust. TODO export it with a 'faust' name
target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI nlohmann_json::nlohmann_json dynamiclib Zep::Zep Tracy::TracyClient)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS) # ImVec2 & ImVec4 math operators
add_definitions(-DZEP_USE_SDL) # Needed for ctrl key to work with Zep
add_definitions(-DCUSTOM_IMGUIFILEDIALOG_CONFIG="../../src/FlowGrid/FileDialog/Config.h") # Relative to `lib/ImGuiFileDialog`
