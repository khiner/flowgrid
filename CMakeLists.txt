cmake_minimum_required(VERSION 3.20)

project(FlowGrid LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
# Manually set `export ASAN_OPTIONS=halt_on_error=0` to not stop after first sanitize issue.
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -L/opt/homebrew/opt/llvm/lib/c++ -L/opt/homebrew/opt/llvm/lib -lunwind -lc++abi")

set(STATIC_FAUST on CACHE BOOL "Build Static Faust library" FORCE) # Dynamic Faust library causes a bus error on macOS.
set(TRACY_ENABLED off CACHE BOOL "Enable Tracy profiling" FORCE)

add_subdirectory(lib)

find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(FFTW3F REQUIRED fftw3f IMPORTED_TARGET)

file(GLOB_RECURSE FlowGridSourceFiles CONFIGURE_DEPENDS src/FlowGrid/*.cpp)
set(ImGuiDir lib/imgui)
set(ImPlotDir lib/implot)
set(ImGuiFileDialogDir lib/ImGuiFileDialog)
set(MaDir lib/miniaudio)
set(TreeSitterDir lib/tree-sitter)
set(TreeSitterGrammarsDir lib/tree-sitter-grammars)

add_executable(${PROJECT_NAME}
    ${ImGuiDir}/imgui_demo.cpp
    ${ImGuiDir}/imgui_draw.cpp
    ${ImGuiDir}/imgui_tables.cpp
    ${ImGuiDir}/imgui_widgets.cpp
    ${ImGuiDir}/imgui.cpp
    ${ImGuiDir}/backends/imgui_impl_vulkan.cpp
    ${ImGuiDir}/backends/imgui_impl_sdl3.cpp
    ${ImGuiDir}/misc/freetype/imgui_freetype.cpp
    ${ImPlotDir}/implot.cpp
    ${ImPlotDir}/implot_items.cpp
    ${ImPlotDir}/implot_demo.cpp
    ${ImGuiFileDialogDir}/ImGuiFileDialog.cpp
    ${MaDir}/extras/miniaudio_split/miniaudio.c
    ${TreeSitterDir}/lib/src/lib.c
    ${TreeSitterGrammarsDir}/tree-sitter-cpp/src/parser.c
    ${TreeSitterGrammarsDir}/tree-sitter-cpp/src/scanner.c
    ${TreeSitterGrammarsDir}/tree-sitter-faust/src/parser.c
    ${TreeSitterGrammarsDir}/tree-sitter-json/src/parser.c
    ${FlowGridSourceFiles}
    src/main.cpp
)

include_directories(
    src/FlowGrid
    ${SDL3_DIR}/include
    ${FREETYPE_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${ImGuiDir}
    ${ImGuiDir}/backends
    ${ImPlotDir}
    ${ImGuiFileDialogDir}
    ${MaDir}/extras/miniaudio_split
    lib/faust/architecture
    lib/range-v3/include
    lib/immer
    lib/concurrentqueue
    lib/date/include/date
    ${TreeSitterDir}/lib/include
    ${TreeSitterDir}/lib/src
    ${TreeSitterGrammarsDir}/tree-sitter-cpp/src
    ${TreeSitterGrammarsDir}/tree-sitter-faust/src
    ${TreeSitterGrammarsDir}/tree-sitter-json/src
    PkgConfig::FFTW3F
)

set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/res")
set(RESOURCE_DEST "${CMAKE_BINARY_DIR}/res")

# Copy resources after building the project target.
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${RESOURCE_DIR} ${RESOURCE_DEST}
    COMMENT "Copying resources to build directory"
)

option(TRACING_ENABLED "Enable Tracy profiling" off)
if(TRACING_ENABLED)
    set(TracyDir lib/tracy)
    set(TRACY_ENABLE on CACHE BOOL "Enable profiling" FORCE)
    set(TRACY_ON_DEMAND off CACHE BOOL "On-demand profiling" FORCE)
    add_subdirectory(${TracyDir})
    include_directories(${TracyDir}/public/tracy)
    target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TRACING_ENABLED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${FREETYPE_LIBRARIES} ${Vulkan_LIBRARIES} SDL3::SDL3 nlohmann_json::nlohmann_json faustlib PkgConfig::FFTW3F)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS) # ImVec2 & ImVec4 math operators
add_definitions(-DIMGUI_ENABLE_FREETYPE)
add_definitions(-DCUSTOM_IMGUIFILEDIALOG_CONFIG="Project/FileDialog/Config.h")
add_definitions(-DMA_NO_ENGINE -DMA_NO_ENCODING -DMA_NO_DECODING)
