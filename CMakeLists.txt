cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid)

add_subdirectory(libsoundio)
add_executable(FlowGrid "${PROJECT_SOURCE_DIR}/sine.c")

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC libsoundio)
target_link_libraries(${PROJECT_NAME} PRIVATE libsoundio_shared)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
