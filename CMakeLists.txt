cmake_minimum_required(VERSION 3.16)

project(FlowGrid)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
find_package(SoundIO REQUIRED)
find_package(Julia REQUIRED)

add_executable(FlowGrid "${PROJECT_SOURCE_DIR}/sine.c" ${Julia_MAIN_HEADER})
target_include_directories(FlowGrid SYSTEM PUBLIC ${SOUNDIO_INCLUDE_DIR} ${Julia_INCLUDE_DIRS})
target_link_libraries(FlowGrid ${SOUNDIO_LIBRARY} ${Julia_LIBRARY})
target_compile_options(FlowGrid PRIVATE -Wall -Wextra)
