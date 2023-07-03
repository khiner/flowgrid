# FlowGrid

Faust is an immediate-mode interface for the [Faust](https://github.com/grame-cncm/faust) audio language.
It is backed by a persistent (as in persistent data structures), fully-undoable store, supporting navigation to any point in the project history in constant time.

My goal with FlowGrid is to create a framework for making artful/(self-)educational/useful interactive audiovisual programs.

_Still actively building this. Expect main to be occasionally broken._

## Application architecture

FlowGrid uses a unidirectional data-flow architecture (like Redux).
Each user action is encapsulated as a simple struct with all the information needed to carry out its effect on the project state.
Actions are grouped together into `std::variant` types composed in a nested domain hierarchy, with a variant type called `Action::Any` at the root.
All issued actions are queued into a single concurrent queue, and each applied action overwrites the project store.
Actions are grouped into "gestures", and each gesture holds a logical snapshot of the full project state after applying its actions.
If you're unfamiliar with persistent data structures, this is much less memory intensive than it sounds!
Each snapshot only needs to track a relatively small amount of data representing its change to the underlying store, a concept referred to as "structural sharing".

This architectural pattern is largely inspired by [Lager](https://github.com/arximboldi/lager) but with fewer bells and whistles and none of its dependencies other than [immer](https://github.com/arximboldi/immer/).

## Application docs

### Project files

FlowGrid supports three project formats.
When saving a project, you can select any of these formats using the filter dropdown in the lower-right of the file dialog.
Each type of FlowGrid project file is saved as plain JSON.

- `.fgs`: _FlowGrid**State**_
  - The full project state.
    An `.fgs` file contains a JSON blob with all the information needed to get back to the saved project state.
    Loading a `.fgs` project file will completely replace the project state with its own.
  - As a special case, the project file `./flowgrid/empty.fgs` (relative to the project build folder) is used internally to load projects.
    This `empty.fgs` file is used internally to implement the `open_empty_project` action, which can be triggered via the `File->New project` menu item, or with `Cmd+n`.
    FlowGrid (over-)writes this file every launch, after initializing to empty-project values (and, currently, rendering two frames to let ImGui fully establish its context).
    This approach provides a pretty strong guarantee that loading a new project will always produce the same, valid empty-project state.
- `.fga`: _FlowGrid**Actions**_
  - FlowGrid can also save and load projects as a list of _action gestures_.
    This format stores an ordered record of _every action_ that affected the project state up to the time it was saved.
    More accurately, an `.fga` file is a list _of lists_ of (action, timestamp) pairs.
    Each top-level list represents a logical _gesture_, composed of a list of actions, along with the absolute time they occurred.
    Each action item contains all the information needed to carry out its effect on the project state.
    In other words, each list of actions in an `.fga` file tells you, in application-domain semantics, what _happened_.
  - **Gesture compression:** Actions within each gesture are compressed down to a potentially smaller set of actions.
    This compression is done in a way that retains the same project state effects, while also keeping the same application-domain semantics.

## Clean/Build/Run

This project uses LLVM IR to JIT-compile Faust code.
To simplify, make things more predictable, and reduce bloat, we use the LLVM ecosystem as much as possible.

We use `clang++/clang` to compile, and LLVM's `lld` for linking.
Even if it's not strictly required, we generally aim to use the newest available LLVM point release (`x.0.1` and beyond).
If the project does not build correctly for you, please make sure your `clang`, `lld`, and `clang-config` point to the newest available point-release of LLVM.
If that doesn't work, try the latest release in the previous LLVM major version.

**VSCode:** The `.vscode/launch.json` config has debug/release launch profiles (todo actually add release profile).

### Mac

- **Install system requirements:**

  ```shell
  $ git clone --recursive git@github.com:khiner/flowgrid.git
  $ brew install cmake pkgconfig llvm freetype
  $ brew link llvm --force
  ```

- Download and install the latest SDK from https://vulkan.lunarg.com/sdk/home
- Set the `VULKAN_SDK` environment variable.
  For example, I have the following line in my `.zshrc` file:
  ```shell
  export VULKAN_SDK="$HOME/VulkanSDK/1.3.250.0/macOS"
  ```

All scripts can be run from anywhere, but to the root repo directory (clean/build).

- **Clean:**
  - Clean up everything: `./script/Clean`
  - Clean debug build only: `./script/Clean -d [--debug]`
  - Clean release build only: `./script/Clean -r [--release]`
- **Build:**
  - Debug build (default): `./script/Build`
  - Release build: `./script/Build -r [--release]`
  - Tracy build: `./script/Build -t [--trace]`

Debug build is generated in the `./build` directory relative to project (repo) root.
Release build is generated in `./build-release`.
Tracy build generated in `./build-tracing`

To run the freshly built application:

```sh
# The application assumes it's being run from the build directory when locating its resource files (e.g. font files).
$ cd build # or build-release
$ ./FlowGrid # application must be run from a directory above root. todo run from anywhere
```

If the build/run doesn't work for you, please [file an issue](https://github.com/khiner/flowgrid/issues/new), providing your environment and any other relevant details, and I will try and repro/fix!

## Stack

### Audio

- [Faust](https://github.com/grame-cncm/faust) for DSP
- [miniaudio](https://github.com/mackron/miniaudio) for the audio backend

### UI/UX

- [ImGui](https://github.com/ocornut/imgui): UI & interactions
- [ImPlot](https://github.com/epezent/implot): plotting
- [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog): file selection
- [ImGuiColorTextEdit](https://github.com/santaclose/ImGuiColorTextEdit): code/text editing
- [ImGui memory_editor](https://github.com/ocornut/imgui_club): viewing/editing memory directly

### Backend

- [immer](https://github.com/arximboldi/immer): persistent data structures for the main project state store
  - Used to quickly create, store, and restore persistent state snapshot
    (used for undo/redo, and for debugging/inspection/monitoring)
- [json](https://github.com/nlohmann/json): state serialization
- [ConcurrentQueue](https://github.com/cameron314/concurrentqueue): the main action queue
  - Actions are _processed_ synchronously on the UI thread, but any thread can submit actions to the queue, via the global `q` method.

### C++ extensions

For C++20 features only partially/experimentally supported in Clang 16:

- [range-v3](https://github.com/ericniebler/range-v3)
  - Only still needed since `std::ranges::to` was pushed to C++23 and isn't supported by clang yet.

### Debugging

- [Tracy](https://github.com/wolfpld/tracy) for real-time profiling

## Development

I try and keep all dependencies up to date.
LLVM version 16+ is required to build.

### Formatting

FlowGrid uses `clang-format` for code formatting.
`./script/Format` formats every cxx file in `src`.

### Tracing

Use `./script/Build -t [--trace]` to create a traced build.

To build and run the [Tracy](https://github.com/wolfpld/tracy) profiler, run:

```sh
$ brew install gtk+3 glfw capstone freetype
$ cd lib/tracy/profiler/build/unix
$ make release
$ ./Tracy-release
```

### Updating submodules

All submodules are in the `lib` directory.

#### Non-forked submodules

Most submodules are not forked.
Here is my process for updating to the tip of all the submodule branches:

```sh
$ git submodule update --remote
$ git add .
$ git cm -m "Update libs"
```

#### Forked submodules

The following modules are forked by me, along with the upstream branch the fork is based on:

- [`imgui:docking`](https://github.com/khiner/imgui/tree/docking)
- [`implot:master`](https://github.com/khiner/implot)
- [`imgui_club:main`](https://github.com/khiner/imgui_club)

I keep my changes rebased on top of the original repo branches.
Here's my process:

```sh
$ cd lib/{library}
$ git pull --rebase upstream {branch} # `upstream` points to the original repo. See list above for the tracked branch
$ ... # Resolve any conflicts & test
$ git push --force
```

## License

This software is distributed under the [GPL v3 License](./LICENSE).

GPL v3 is a strong copyleft license, which basically means any copy or modification of the code in this repo (excluding any libraries in the `lib` directory with different licenses) must also be released under the GPL v3 license.

### Why copyleft?

The audio world has plenty of open-source resources, but proprietary intellectual property dominates the commercial audio software industry.

A permissive license allowing closed-source commercial usage may help more end users (musicians, artists, creators) in the short term, but it doesn't help developers.
As a music producer, finding excellent software or hardware is relatively easy.
As a developer, however, I've had a much harder time finding resources, tools, and strategies for effective audio software development.

Although this project is first and foremost a creative tool, the intention and spirit is much more about hacking, learning, educating and researching than it is about producing end media products.
For these purposes, keeping the information open is more important than making the functionality freely and widely available.
