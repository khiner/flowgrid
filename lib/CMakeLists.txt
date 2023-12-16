#=================== SDL3 ===================

set(SDL3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL)
add_subdirectory(${SDL3_DIR})

#=================== json ===================

add_subdirectory(json)

#=================== faust ===================
# See [the docs](https://faustdoc.grame.fr/manual/embedding/#using-libfaust-with-the-llvm-backend) for general help.

set(INCLUDE_EXECUTABLE off CACHE BOOL "Include runtime executable" FORCE)
set(INCLUDE_OSC off CACHE BOOL "Include Faust OSC library" FORCE)
set(INCLUDE_HTTP off CACHE BOOL "Include Faust HTTPD library" FORCE)
set(INCLUDE_WASM_GLUE off CACHE BOOL "Include wasm glue targets" FORCE)
set(INCLUDE_EMCC off CACHE BOOL "Include emcc targets" FORCE)

option(STATIC_FAUST "Build static Faust library (`off` to build dynamic library)" on)
if(STATIC_FAUST)
    message(STATUS "Building static faustlib")
    set(INCLUDE_STATIC on CACHE BOOL "Include static Faust library" FORCE)
else()
    message(STATUS "Building dynamic faustlib")
    set(INCLUDE_DYNAMIC on CACHE BOOL "Include dynamic Faust library" FORCE)
endif()

set(LLVM_BACKEND COMPILER STATIC DYNAMIC CACHE STRING "Include LLVM backend" FORCE)

include_directories(faust/compiler/generator/c)
include_directories(faust/compiler/generator/cpp)
add_subdirectory(faust/build EXCLUDE_FROM_ALL)

# Create the `faustlib` target.
if(STATIC_FAUST)
    set(FAUST_TARGET_LIB "staticlib")
else()
    set(FAUST_TARGET_LIB "dynamiclib")
endif()
target_compile_definitions(${FAUST_TARGET_LIB} PUBLIC LLVM_BUILD_UNIVERSAL=1)
add_library(faustlib ALIAS ${FAUST_TARGET_LIB})
