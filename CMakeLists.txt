cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)

project(FlowGrid LANGUAGES C CXX)

add_subdirectory("lib")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.h src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(lib/faust/architecture)
include_directories(lib/faust/compiler)
include_directories(lib/concurrentqueue)
include_directories(lib/zep/include)
include_directories(lib/diff-match-patch-cpp-stl)

# `dynamiclib` is faust. TODO export it with a 'faust' name
target_link_libraries(${PROJECT_NAME} PRIVATE IMGUI libsoundio_shared nlohmann_json::nlohmann_json dynamiclib Zep::Zep)
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
