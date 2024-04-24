# FlowGrid

FlowGrid is an immediate-mode interface for the [Faust](https://github.com/grame-cncm/faust) audio language.
It is backed by a persistent (as in persistent data structures), fully-undoable store, supporting navigation to any point in the project history in constant time.

My goal with FlowGrid is to create a framework for making artful, educational, and useful interactive audiovisual programs.

_Still in its early stages. Expect things to be broken!_

FlowGrid aims to be a fast, effective, and intuitive tool for creative real-time audiovisual generation and manipulation, facilitating a joyful, creative, and explorative flow state (half of FlowGrid's namesake).
My ambition for the project is to develop a visual programming language for the creative generation and manipulation of media/data/network/other streams.

The "Grid" half of FlowGrid refers to an ambition to use a (virtually infinite) nested grid of subgrids as the primary UX paradigm.
This idea stemmed from considering how to maximize the expressive power of a single Push 2 controller (or any grid-like controller, such as many from Akai, equipped with an LED and/or paired with a screen).
I haven't gotten to this aspect yet here, but I explored it in [a previous JUCE application](https://github.com/khiner/flowgrid_old) before starting fresh with this project with a new stack and development goals.

_Note: Work here is paused to focus on a Vulkan Mesh Editor and a Rigid Body audio model ([MeshEditor](https://github.com/khiner/MeshEditor))._

## Goals

- **Be fast:**
  - **Low latency:** Minimize the duration between the time the application receives an input signal, and the corresponding output device respond time (e.g., pixels on screen, audio output, file I/O).
    Note that low latency applies to _all_ types of input, which currently include:
      - Audio signals from any audio input device on your machine, at any natively-supported sample rate.
      - Mouse/keyboard input.
      - _TODOs:_ MIDI, most likely using [libremidi](https://github.com/celtera/libremidi) as the backend, targeting Push 2 first (see [Old-FlowGrid implementation](https://github.com/khiner/flowgrid_old/tree/main/src/push2)). USB (including writing to LED displays - see [Old-FlowGrid implementation](https://github.com/khiner/flowgrid_old/blob/main/src/usb/UsbCommunicator.h) but will rewrite from scratch since the API has likely changed and it wasn't rock-solid anyway). OSC (Open Sound Control). WebSockets.
  - **Fast random access to application state history:**
    FlowGrid uses [persistent data structures](https://github.com/arximboldi/lager) to store its state.
    After each [action](#Application-state-architecture), FlowGrid creates a snapshot of the application store and adds it to the history (which will eventually be a full navigation tree), allowing for _constant-time_ navigation to _any point_ in the history.
    In most applications, if a user e.g. just performed their 10th action and wants to go back to where they were after their first action, they would either manually undo 9 times, or if a random access interface is provided, the application would do this under the hood (in linear time, like rewinding a tape).
    FlowGrid, on the other hand, provides navigating to _any point in the application history_ (almost always*) at _frame rate or faster_.
      This opens up many potential creative applications that are not possible with other applications, like, say, muting the audio output device, and then issuing `[undo, redo]` actions at audio rate, for a makeshift square wave generator!

    _\* Some kinds of state changes have higher effect latency, like changing an audio IO device._
  - **Fast rebuild:** Keeping build times low is crucial, as full rebuilds are frequent.
    Minimizing the duration between the edit time of a valid FlowGrid source-code file and the application start time after recompilation is crucial.
    Making compilation snappy isn't just about saving the extra seconds waiting. It's about minimizing the feedback loop between ideation and execution, and making the _process_ of building more engaging.
    _FlowGrid is currently not great in this department and I want to make recompile times faster._
- **Be Effective:**
  - **Be deterministic:** The state should, as much as possible, _fully specify_ the current application instance at any point in time. Closing and opening a project should continue all streams where they left off, including the `DrawData` stream ImGui uses to render its viewport(s).
- **Be intuitive:**
  - _TODO: Expand on this goal._


## Application State Architecture

FlowGrid uses a unidirectional data-flow architecture, similar to Redux.
The architecture draws heavy inspiration from [Lager](https://github.com/arximboldi/lager), albeit with fewer features and dependencies, with [immer](https://github.com/arximboldi/immer/) being the sole exception.

User actions are encapsulated in plain structs, containing all necessary information to update the project state.
Actions are organized into `std::variant` types, arranged in a nested domain hierarchy.
At the root of this structure is a variant type named `Action::Any`.
All actions are enqueued into a single concurrent queue, with each action applied subsequently overwriting the project store.
Actions are grouped by their relative queue time and type, merging into "gestures" at the time of commitment.
Each gesture represents a coherent, undoable group of actions.
Every committed gesture is recorded in a "history record," which includes the gesture itself and a _logical snapshot_ of the entire project state resulting from its application.
If you're unfamiliar with persistent data structures, this is much less memory intensive than it sounds!
Each snapshot only needs to track a relatively small amount of data representing its changes to the underlying store, a concept referred to as "structural sharing".

## Application docs

### Project files

FlowGrid supports two project formats.
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

### Features

TODO

## Clean/Build/Run

This project uses LLVM IR to JIT-compile Faust code.
To simplify, make things more predictable, and reduce bloat, we use the LLVM ecosystem as much as possible - `clang++/clang` to compile, and LLVM's `lld` for linking.
Even if it's not strictly required, I generally aim to use the latest LLVM release available on HomeBrew.
If the project does not build correctly for you, please make sure your `clang`, `lld`, and `clang-config` point to the newest available point-release of LLVM.
If that doesn't work, try the latest release in the previous LLVM major version.

### Mac

- **Install system requirements:**

  ```shell
  $ git clone --recursive git@github.com:khiner/flowgrid.git
  $ brew install cmake pkgconfig llvm freetype fftw
  $ brew link llvm --force
  ```

- Download and install the latest SDK from https://vulkan.lunarg.com/sdk/home
- Set the `VULKAN_SDK` environment variable.
  For example, add the following to your `.zshrc` file:
  ```shell
  export VULKAN_SDK="$HOME/VulkanSDK/{version}/macOS"
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
$ ./FlowGrid # Must be run from a directory above root. todo run from anywhere
```

If the build/run doesn't work for you, please [file an issue](https://github.com/khiner/flowgrid/issues/new), providing your environment and any other relevant details, and I will try and repro/fix!

## Stack

### Audio

- [Faust](https://github.com/grame-cncm/faust) for DSP
- [miniaudio](https://github.com/mackron/miniaudio) for the audio backend
- [fftw](https://www.fftw.org/) for computing spectrograms (visualized with ImPlot)

### UI

- [ImGui](https://github.com/ocornut/imgui) + [SDL3](https://github.com/libsdl-org/SDL) + [Vulkan](https://www.vulkan.org/) + [FreeType](https://freetype.org): UI
  - Using fonts: [Inter 4.0](https://github.com/rsms/inter/releases/tag/v4.0) for regular text and [JetBrains Mono 2.304](https://www.jetbrains.com/lp/mono/) for monospace
- [ImPlot](https://github.com/epezent/implot): plotting
- [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog): file selection

### Backend

- [immer](https://github.com/arximboldi/immer): persistent data structures for the main project state store
  - Used to quickly create, store, and restore persistent state snapshot
    (used for undo/redo, and for debugging/inspection/monitoring)
- [json](https://github.com/nlohmann/json): state serialization
- [tree-sitter](https://github.com/tree-sitter/tree-sitter): Language parsing for syntax highlighting in text buffers
- [ConcurrentQueue](https://github.com/cameron314/concurrentqueue): the main action queue
  - Actions are _processed_ synchronously on the UI thread, but any thread can submit actions to the queue.

### C++ extensions

- [range-v3](https://github.com/ericniebler/range-v3)
  - Only still needed since `std::ranges::concat` was pushed to C++26 and isn't supported by clang yet, (also `std::views::join_with`)

### Debugging

- [Tracy](https://github.com/wolfpld/tracy) for real-time profiling

## Development

I try and keep all dependencies up to date.
LLVM 18 is required to build.

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
- [`miniaudio:dev`](https://github.com/khiner/miniaudio)

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
