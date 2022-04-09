#=================== libsoundio ===================

add_subdirectory(libsoundio)
target_include_directories(libsoundio_shared PUBLIC libsoundio)

#=================== SDL2 ===================

cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(SDL2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SDL)
add_subdirectory(${SDL2_DIR})

#=================== IMGUI ===================

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_library(IMGUI STATIC)

target_sources(IMGUI
    PRIVATE
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui.cpp

    PRIVATE
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl.cpp
    )

target_include_directories(IMGUI
    PUBLIC ${IMGUI_DIR}
    PUBLIC ${IMGUI_DIR}/backends
    PUBLIC ${SDL2_DIR}/include
    )

find_package(OpenGL REQUIRED)
target_link_libraries(IMGUI PUBLIC ${OPENGL_LIBRARIES})
target_link_libraries(IMGUI PUBLIC SDL2main SDL2 ${CMAKE_DL_LIBS})

#=================== json ===================

add_subdirectory(json)

#=================== faust ===================

# See [the docs](https://faustdoc.grame.fr/manual/embedding/#using-libfaust-with-the-llvm-backend)
# for general help.

# This config for building faust as a dynamic library is based on
# [TD-Faust](https://github.com/DBraun/TD-Faust/blob/965963258b2c68054fc7e376e1fa84ea19ce7af4/FaustLib.cmake#L26-L62).
# The libfaust usage for TD-Faust is [here](https://github.com/DBraun/TD-Faust/blob/c31cc7d5fd336b663396ed87f7692f82820e7b20/TD-Faust/FaustCHOP.h).

# TODO only if using homebrew? maybe just find a way to remove this, and handle with local run env vars
set(PKG_CONFIG_EXECUTABLE /opt/homebrew/bin/pkg-config)

set(MSVC_STATIC OFF CACHE STRING "Use static runtimes with MSVC" FORCE)
set(INCLUDE_STATIC OFF CACHE STRING "Include static library" FORCE)
set(INCLUDE_EXECUTABLE ON CACHE STRING "Include runtime executable" FORCE)
set(INCLUDE_DYNAMIC ON CACHE STRING "Include dynamic library" FORCE)
set(INCLUDE_OSC OFF CACHE STRING "Include Faust OSC library" FORCE)
set(INCLUDE_HTTP OFF CACHE STRING "Include Faust HTTPD library" FORCE)

set(ASMJS_BACKEND OFF CACHE STRING "Include ASMJS backend" FORCE)
set(C_BACKEND OFF CACHE STRING "Include C backend" FORCE)
set(CPP_BACKEND DYNAMIC CACHE STRING "Include CPP backend" FORCE)
set(FIR_BACKEND OFF CACHE STRING "Include FIR backend" FORCE)
set(INTERP_BACKEND OFF CACHE STRING "Include INTERPRETER backend" FORCE)
set(JAVA_BACKEND OFF CACHE STRING "Include JAVA backend" FORCE)
set(JS_BACKEND OFF CACHE STRING "Include JAVASCRIPT backend" FORCE)
set(LLVM_BACKEND COMPILER DYNAMIC CACHE STRING "Include LLVM backend" FORCE)
set(OLDCPP_BACKEND OFF CACHE STRING "Include old CPP backend" FORCE)
set(RUST_BACKEND OFF CACHE STRING "Include RUST backend" FORCE)
set(WASM_BACKEND OFF CACHE STRING "Include WASM backend" FORCE)

add_subdirectory(faust/build EXCLUDE_FROM_ALL)
target_compile_definitions(dynamiclib PUBLIC LLVM_BUILD_UNIVERSAL=1)

#=================== zep ===================
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(FREETYPE_DIR /opt/homebrew/opt/freetype)
find_package(Freetype REQUIRED)

set(BUILD_IMGUI OFF CACHE STRING "Make Imgui Library" FORCE)
set(BUILD_DEMOS OFF CACHE STRING "Make the demo app" FORCE)
set(BUILD_TESTS OFF CACHE STRING "Make the tests" FORCE)
set(BUILD_EXTENSIONS OFF CACHE STRING "Make the extension library (required for demo)" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zep)
