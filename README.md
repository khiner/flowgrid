# FlowGrid

Prototyping the new stack for FlowGrid.
(Old version [here](https://github.com/khiner/flowgrid))

I'm still actively building this.
Currently, I'm basically trying to maximally mash together some wonderful libraries (see [**Stack**](#stack)):

## Build and run

### Mac

```shell
$ git clone --recursive git@github.com:khiner/flowgrid2.git
$ brew install pkgconfig llvm freetype
$ brew link llvm --force
```

TODO: Will probably want to build llvm locally as a submodule, and point to it.
See [TD-Faust](https://github.com/DBraun/TD-Faust/blob/02f35e4343370559c779468413c32179f55c6552/build_macos.sh#L5-L31)
as an example.

## Stack

### Audio

* [Faust](https://github.com/grame-cncm/faust) for DSP
* [libsoundio](https://github.com/andrewrk/libsoundio) for the audio backend

### UI/UX

* [ImGui](https://github.com/ocornut/imgui) for UI
* [zep](https://github.com/Rezonality/zep) for code/text editing

### Backend

* [json](https://github.com/nlohmann/json) for state serialization, and for the diff-patching mechanism behind undo/redo
* [ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for the main event queue
* [diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl) for diff-patching on unstructured
  text

### Development

* [Tracy](https://github.com/wolfpld/tracy) for real-time profiling

### Tracing

To enable tracing, set `TRACY_ENABLE` to `ON` in the main project `CMakeLists.txt`.

To build and run the [Tracy](https://github.com/wolfpld/tracy) profiler,

```shell
$ brew install gtk+3 glfw capstone freetype
$ cd lib/tracy/profiler/build/unix
$ make release
$ ./Tracy-release
```
