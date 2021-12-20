cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

add_subdirectory("lib")

add_executable(
    ${PROJECT_NAME}
    src/action.h src/audio.h src/context.h src/draw.h src/state.h src/update.h src/visitor.h
    src/update.cpp src/draw.cpp src/audio.cpp src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI libsoundio_shared nlohmann_json::nlohmann_json)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
