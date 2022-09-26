# FlowGrid

_Still actively building this._

## Application goals

My goal with FlowGrid to create artful/(self-)educational/useful interactive audiovisual programs.

## Current development goals

After learning a lot of lessons developing the [first iteration](https://github.com/khiner/flowgrid_old) of this project, I'm starting from scratch with a [new stack](#Stack) and expanded scope.

So far, I'm basically trying to mash together some great libraries.
Here are some of my current development thoughts/goals, roughly broken up into abstract/concrete:

### Abstract development goals

* **Keep it as simple as I possibly can.**
* Focus on making it fun to use _and create_ the application.
* Spend more time up front getting the foundation right (simple, transparent, flexible, powerful)
* Keep (re-)build times low.
* At this early stage, my main application goal is to _facilitate the development of the app_.
  - Invest early in things like adding debugging capabilities
  - Make the application state and context transparent and easily modifiable
  - Provide performance metrics in real-time
* Let myself optimize to some extent.
  In computers, fast things are good and always more fun than slow things.
* Prioritize learning over development velocity.
  Dig into, and take ownership over, low-level concerns where appropriate.
  Feed curiosity.
  Bias towards reinventing wheels over accepting bloated/complex dependencies.

### Concrete development goals

* Store the source-of-truth application state in a single `struct` with global read access.
* Perform all actions that affect the application state in one place.
* Provide global read access to all application runtime state
* Make _everything_ undo/redo-able.
* As much as possible, make the UI a pure function of the application state.

The main architecture patterns for this app are inspired by [Lager's](https://github.com/arximboldi/lager) value-oriented design and unidirectional data-flow architecture.
Lager, in turn, is inspired by frameworks like [Elm](https://guide.elm-lang.org/architecture) and [Redux](https://redux.js.org/introduction/getting-started).
I don't actually use lager in this project, however, since I find it to be too complex.
Given how fundamental state management is, I'd prefer to understand as much as possible about how it's implemented, and I want to avoid any layers of abstraction.

Rather than using proper [persistent data structures](https://github.com/arximboldi/immer), FlowGrid uses regular old C++ data types & `std` data structures, and records state diffs by [computing a JSON diff](https://github.com/nlohmann/json#json-pointer-and-json-patch) after each action.
This achieves basically the same thing, and trades lower complexity for (generally) more expensive state updates.

## Build and run

### Mac

#### System requirements

Prepare your environment:

```sh
$ git clone --recursive git@github.com:khiner/flowgrid.git
$ brew install cmake pkgconfig llvm freetype
$ brew link llvm --force
```

#### IDE clean/build/run

* **CLion:** I use CLion to develop this application, so that's the only IDE I can attest to working smoothly.
  You should be able to just open the project in CLion and run the saved `FlowGrid | Debug` configuration.
* **VSCode:** I don't actively use VSCode for FlowGrid (although I would if its refactoring capabilities were stronger).
  However, last I tried, I was able to build and ran the app just fine by simply opening the project in the usual way, and using the CMake extension.

#### Manual clean/build/run

Clean and rebuild the project:

```sh
$ rm -rf cmake-build-debug # clean
$ cmake -B cmake-build-debug # create
$ cmake --build cmake-build-debug --target FlowGrid -- -j 8 # make
```

The `rebuild` script does exactly this:

```sh
$ ./rebuild
```

The application should now be fully rebuilt and ready to run.
If this isn't the case for you, please [file an issue](https://github.com/khiner/flowgrid/issues/new), providing your environment and any other relevant details, and I will try and fix it!

To run the freshly (re-)built application:

```sh
# The application assumes it's being run from the build directory when locating its resource files (e.g. font files).
$ cd make-build-debug
$ ./FlowGrid
```

## [**Stack**](#stack)

### Audio

* [Faust](https://github.com/grame-cncm/faust) for DSP
* [libsoundio](https://github.com/andrewrk/libsoundio) for the audio backend, and for its memory-mapped ring buffer, used to buffer audio input and resampling
* [r8brain-free-src](https://github.com/avaneev/r8brain-free-src/) for audio resampling, currently only used when monitoring an audio input stream with a different sample rate than the output stream

### UI/UX

* [ImGui](https://github.com/ocornut/imgui) for UI
* [ImPlot](https://github.com/epezent/implot) for plotting
* [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) for file selection
* [zep](https://github.com/Rezonality/zep) for code/text editing
* [ImGui memory_editor](https://github.com/ocornut/imgui_club) for viewing/editing memory directly

### Backend

* [json](https://github.com/nlohmann/json) for
  - State serialization
  - A path-addressable state-mirror, and
  - The diff-patching mechanism behind undo/redo
  - Probably other things.
    Look for usages of the `json Context::state_json` variable, or its global read-only alias, `const json &sj`.
* ~~[ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for the main event queue~~
  -For now, I just moved this action processing work to the UI thread to avoid issues with concurrent reads/writes to complex structures like JSON
* ~~[diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl) for diff-patching on unstructured
  text~~
  - Was using this to handle ImGui `.ini` settings string diffs, but those are now deserialized into the structured state.
    I'll likely be using this again at some point, to adapt [hlohmann json patches](https://github.com/nlohmann/json#json-pointer-and-json-patch)
    into something like [jsondiffpatch's deltas](https://github.com/benjamine/jsondiffpatch/blob/master/docs/deltas.md#text-diffs),
    for unified handling of state diffs involving long text strings (like code strings).

### C++ extensions

* [range-v3](https://github.com/ericniebler/range-v3), since ranges are only partially supported in Clang 14
* [fmt](https://github.com/fmtlib/fmt) for C++20-style string formatting

### Debugging

* [Tracy](https://github.com/wolfpld/tracy) for real-time profiling

## Development

### Tracing

To enable tracing, set `TRACY_ENABLE` to `ON` in the main project `CMakeLists.txt`.

To build and run the [Tracy](https://github.com/wolfpld/tracy) profiler,

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
$ git cm -m "Bump libs"
```

#### Forked submodules

The following modules are [forked by me](https://github.com/khiner?tab=repositories&q=&type=fork), along with the upstream branch the fork is based on:
* `imgui:docking`
* `implot:master`
* `libsoundio:master`
* `zep:master`
* `imgui_club:master`

I keep my changes rebased on top of the original repo branches.
Here's my process:

```sh
$ cd lib/{library}
$ git pull --rebase upstream {branch} # `upstream` points to the original repo. See list above for the tracked branch
$ ... # Resolve any conflicts & test
$ git push --force
```

A notable exception is my zep fork, which has so many changes that almost no upstream commits will rebase successfully.
The way I handle rebasing against zep is to rebase one commit at a time, using `--strategy-option theirs` (`-Xtheirs`), manually resolving any rebase conflicts:

```sh
$ cd lib/zep
$ git pull --rebase -Xtheirs upstream {commit_sha}
$ ... # Resolve any conflicts, port any missing changes manually, verify...
$ git push --force
```

## Application docs

### Project files

FlowGrid supports three project formats.
When saving a project, you can select any of these formats using the filter dropdown in the lower-right of the file dialog.
Each type of FlowGrid project file is saved as [MessagePack-encoded JSON](https://github.com/nlohmann/json#binary-formats-bson-cbor-messagepack-ubjson-and-bjdata) _(TODO: provide preferences toggle for MessagePack-encoding)_:

* `.fgs`: _FlowGrid**State**_
  - The full application state.
    An `.fgs` file contains a JSON blob with all the information needed to get back to the saved project state.
    Loading a `.fgs` project file will completely replace the application state with its own.
  - As a special case, the project file `./flowgrid/empty.fgs` (relative to the project build folder) is used internally to load projects.
    This `empty.fgs` file is used internally to implement the `open_empty_project` action, which can be triggered via the `File->New project` menu item, or with `Cmd+n`.
    FlowGrid (over-)writes this file every launch, after initializing to empty-project values (and, currently, rendering two frames to let ImGui fully establish its context).
    This approach provides a pretty strong guarantee that loading a new project will always produce the same, valid empty-project state.
* `.fgd`: _FlowGrid**Diffs**_
  - Instead of saving the full application state, `.fgd` project files store a JSON object with two properties:
    *`diffs`: A list of _bidirectional project state diffs_ (each with forward- and reverse-patches), in [JSON Patch](https://jsonpatch.com/) format, corresponding to the application-state effects of every action that has effected the application's state since its launch
    *`diff_index`: The project's position in the list of diffs. (Or, equivalently, action position or position in the undo-stack)
    When you save your project as an `.fgd` file, your current undo-position is stored here.
  - FlowGrid loads `.fgd` project files by:
    * Running the `open_empty_project` action (explained above) to clear the current application and load a fresh empty one
    * Setting the application context's `diffs` list to the one stored in the file
    * Executing each (forward-)diff up to the stored `diff_index`
* `.fga`: _FlowGrid**Actions**_
  - Finally, FlowGrid can save and load projects as a list of _action gestures_.
    This format stores an ordered record of _every action_ that affected the app state up to the time it was saved.
    More accurately, an `.fga` file is a list _of lists_ of actions.
    Each top-level list represents a logical _gesture_, composed of a list of actions.
    Each action item contains all the information needed to carry out its effect on the application state.
    Contrast this with `.fgd` project files, which store the _results_ of each action over the application session.
    Each list of actions in a `.fga` file tells you, in application-domain semantics, what _happened_.
    A diff item in a `.fgd` file tells you what _changed_ as a result.
  - **Gesture compression:** Actions within each gesture are compressed to a potentially smaller set of actions.
    This compression is done in a way that retains the same application-state effects, while keeping the same application-domain semantics.
  - FlowGrid loads `.fga` project files by:
    * Running the `open_empty_project` action
    * Executing each action stored in the file, finalizing the gesture after each sub-list.

_TODO: Tradeoffs between project types_

## License

This software is distributed under the [GPL v3 License](./LICENSE).

GPL v3 is a strong copyleft license, which basically means any copy or modification of the code in this repo (excluding any libraries in the `lib` directory with different licenses) must also be released under the GPL v3 license.

### Why copyleft?

The audio world has lots of high quality open-source code, educational resources, libraries, and other software resources.
However, the commercial audio software industry is also full of protected IP.
This is a necessary strategy for the too-few companies that manage to achieve some level of financial independence in an industry where that's a hard thing to do.

Choosing a permissive license allowing for closed-source commercial usage may stand to benefit more end users (musicians, artists, creators) in the short-term, since companies producing closed-source software could freely put the code right directly their products without violating copyright.

I usually find it pretty easy to find the music software or hardware I need as a _music producer_.
As a _developer_, I've found it much harder to find the resources, tooling, and methodologies I need to easily create effective new audio software.

Although this project is first and foremost a creative tool, the intention and spirit is much more about hacking, learning, educating and researching than it is about producing end media products.
For these purposes, keeping the information open is more important than making the functionality freely and widely available.
