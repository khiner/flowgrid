cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

add_subdirectory("lib")

add_executable(
    ${PROJECT_NAME}
    src/action.h src/audio.h src/context.h src/draw.h src/state.h src/visitor.h
    src/draw.cpp src/audio.cpp src/main.cpp src/transformers/bijective/state2json.cpp src/transformers/bijective/state2json.cpp src/context.cpp src/transformers/bijective/json2state.cpp src/transformers/bijective/json2state.h)

set(FAUST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/faust)
include_directories(${FAUST_DIR}/architecture)
include_directories(${FAUST_DIR}/compiler)
include_directories(${FAUST_DIR}/compiler/utils)
add_dependencies(${PROJECT_NAME} dynamiclib)

set(CONCURRENTQUEUE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/concurrentqueue)
include_directories(${CONCURRENTQUEUE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI libsoundio_shared nlohmann_json::nlohmann_json dynamiclib)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
