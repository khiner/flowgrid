cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

add_subdirectory("lib")

add_executable(
    ${PROJECT_NAME}
    src/action.h src/audio.h src/context.h src/draw.h src/state.h src/visitor.h
    src/draw.cpp src/audio.cpp src/main.cpp src/transformers/bijective/state2json.cpp src/transformers/bijective/state2json.cpp src/context.cpp src/transformers/bijective/json2state.cpp src/transformers/bijective/json2state.h src/process_manager.cpp src/process_manager.h src/config.h src/audio_context.cpp src/audio_context.h src/editor.cpp src/editor.h)

set(FAUST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/faust)
include_directories(${FAUST_DIR}/architecture)
include_directories(${FAUST_DIR}/compiler)
include_directories(${FAUST_DIR}/compiler/utils)
add_dependencies(${PROJECT_NAME} dynamiclib)

set(CONCURRENTQUEUE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/concurrentqueue)
set(ZEP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/zep)
include_directories(${CONCURRENTQUEUE_DIR})
include_directories(${ZEP_DIR}/include)
#include_directories(${ZEP_DIR}/src)

# dynamiclib is faust. TODO export it with a 'faust' name
target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI libsoundio_shared nlohmann_json::nlohmann_json dynamiclib Zep::Zep)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
