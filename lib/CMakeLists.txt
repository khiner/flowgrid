#=================== SDL3 ===================

set(SDL3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL)
add_subdirectory(${SDL3_DIR})

#=================== json ===================

add_subdirectory(json)

#=================== faust ===================
# See [the docs](https://faustdoc.grame.fr/manual/embedding/#using-libfaust-with-the-llvm-backend) for general help.

set(INCLUDE_EXECUTABLE OFF CACHE STRING "Include runtime executable" FORCE)
set(INCLUDE_OSC OFF CACHE STRING "Include Faust OSC library" FORCE)
set(INCLUDE_HTTP OFF CACHE STRING "Include Faust HTTPD library" FORCE)
set(INCLUDE_WASM_GLUE OFF CACHE STRING "Include wasm glue targets" FORCE)
set(INCLUDE_EMCC OFF CACHE STRING "Include emcc targets" FORCE)

set(INCLUDE_DYNAMIC ON CACHE STRING "Include dynamic library" FORCE)
set(LLVM_BACKEND COMPILER DYNAMIC CACHE STRING "Include LLVM backend" FORCE)

include_directories(faust/compiler/generator/c)
include_directories(faust/compiler/generator/cpp)
add_subdirectory(faust/build EXCLUDE_FROM_ALL)
target_compile_definitions(dynamiclib PUBLIC LLVM_BUILD_UNIVERSAL=1)

#=================== zep ===================
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(FREETYPE_DIR /opt/homebrew/opt/freetype)
find_package(Freetype REQUIRED)

set(BUILD_TESTS OFF CACHE STRING "Make the tests" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zep)
