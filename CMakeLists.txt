cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid)

add_subdirectory("lib/libsoundio")
add_executable(${PROJECT_NAME} "src/sine.c")

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC lib/libsoundio)
target_link_libraries(${PROJECT_NAME} PRIVATE libsoundio_shared)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

# TODO use https://github.com/Pesc0/imgui-cmake as an example
# TODO brew uninstall SDL (only use the submodule)
