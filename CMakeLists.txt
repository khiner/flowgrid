cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

add_subdirectory("lib")

file(GLOB flowgrid_SRC
    "src/*.h"
    "src/*.cpp"
)

add_executable(${PROJECT_NAME} ${flowgrid_SRC})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC lib/libsoundio)
target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI libsoundio_shared)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
