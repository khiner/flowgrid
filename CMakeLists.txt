cmake_minimum_required(VERSION 3.20)
project(FlowGrid LANGUAGES C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -stdlib=libc++ -fexperimental-library")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

add_subdirectory(lib)

set(IMGUI_DIR lib/imgui)
set(IMPLOT_DIR lib/implot)

file(
    GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
    lib/ImGuiFileDialog/ImGuiFileDialog.cpp
)

find_package(OpenGL REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(
    ${SDL3_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    lib/miniaudio
    lib/faust/architecture
    lib/ImGuiFileDialog
    lib/imgui_club/imgui_memory_editor
    lib/range-v3/include
    lib/immer
    lib/concurrentqueue
    lib/date/include/date
)

option(TRACING_ENABLED "Enable Tracy profiling" OFF)
if(TRACING_ENABLED)
    set(TRACY_ENABLE ON CACHE STRING "Enable profiling" FORCE)
    set(TRACY_ON_DEMAND OFF CACHE STRING "On-demand profiling" FORCE)
    add_subdirectory(lib/tracy)
    include_directories(lib/tracy/public/tracy)
    target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACING_ENABLED)
endif()

# `dynamiclib` is faust. TODO export it with a 'faust' name
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL SDL3::SDL3 nlohmann_json::nlohmann_json dynamiclib)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS) # ImVec2 & ImVec4 math operators
add_definitions(-DCUSTOM_IMGUIFILEDIALOG_CONFIG="../../src/FlowGrid/FileDialog/Config.h") # Relative to `lib/ImGuiFileDialog`
