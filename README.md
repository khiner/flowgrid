# FlowGrid

Still actively building this.

## Current development goals

After learning a lot of lessons developing the [first iteration](https://github.com/khiner/flowgrid_old) of this project, I'm starting from scratch with a new stack and a clearer vision.

So far, I'm basically trying to mash together some great libraries.

Here are some of my current development thoughts/goals, roughly broken up into abstract/concrete:

### Abstract development goals

* Focus on making it fun to use _and create_ this application.
* Spend more time upfront trying to get simple and flexible foundations right
* Try to keep (re-)build times low.
* Early on, the main function of the app should be to _facilitate the development of the app_.
  Invest early in things like debugging capabilities, making the application state transparent, providing metrics, measuring performance, etc.
* Let myself optimize to some extent.
  In computers, fast things are good and always more fun than slow things.
* Prioritize learning over development velocity.
  Dig into, and take ownership over, low-level concerns where appropriate.
  Feed curiosity.
  Bias towards reinventing wheels over accepting bloated/complex/overly-featured dependencies.

### Concrete development goals

* Store the source-of-truth application state in a single `struct` with global read access.
* Perform all actions that affect the application state in one place.
* Provide global read access to all application runtime state
* Make _everything_ undo/redo-able.

The main architecture patterns for this app are inspired by [lager's](https://github.com/arximboldi/lager) value-oriented design and unidirectional data-flow architecture.
Lager, in turn, is inspired by frameworks like [Elm](https://guide.elm-lang.org/architecture) and [Redux](https://redux.js.org/introduction/getting-started).
I don't actually use lager in this project, however, since I find it to be too complex.
Given how fundamental state management is, I'd prefer to understand as much as possible about how it's implemented, and I want to avoid any layers of abstraction.

Rather than using proper [persistent data structures](https://github.com/arximboldi/immer), FlowGrid uses regular old C++ data types & `std` data structures, and records state diffs by [computing a JSON diff](https://github.com/nlohmann/json#json-pointer-and-json-patch) after each action.

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
* **TODO:** anything needed for a VSCode project? would be nice to be able to assess it since most refactoring seems weirdly slow in CLion, and doesn't support many features in other JetBrains products.

#### Manual clean/build/run

Clean and rebuild the project:

```sh
$ rm -rf cmake-build-debug # clean
$ cmake -B cmake-build-debug # create
$ cmake --build cmake-build-debug --target FlowGrid -- -j 8 # make
```

Or, you could just run the `rebuild` script for the same effect:

```sh
$ ./rebuild
```

The application should now be fully rebuilt and ready to run.
If this isn't the case for you, please report it to me along with your environment and any other relevant details!

To run the freshly (re-)built application:

```sh
# The application assumes it's being run from the build directory,
# and makes assumptions about the locations of its resource files (e.g. font files).
$ cd make-build-debug
$ ./FlowGrid
```

## [**Stack**](#stack)

### Audio

* [Faust](https://github.com/grame-cncm/faust) for DSP
* [libsoundio](https://github.com/andrewrk/libsoundio) for the audio backend

### UI/UX

* [ImGui](https://github.com/ocornut/imgui) for UI
* [ImPlot](https://github.com/epezent/implot) for plotting
* [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) for file selection
* [zep](https://github.com/Rezonality/zep) for code/text editing
* [fmt](https://github.com/fmtlib/fmt) for C++20-style string formatting. Currently, only used for time-formatting.

### Backend

* [json](https://github.com/nlohmann/json) for state serialization, and for the diff-patching mechanism behind undo/redo
* ~~[ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for the main event queue~~
  -For now just moved this action processing work to the UI thread to avoid issues with concurrent reads/writes to complex structures like JSON)
* ~~[diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl) for diff-patching on unstructured
  text~~
  - Was using to handle ImGui `.ini` settings string diffs, but those are now deserialized into the structured state.
    Will be using this again soon, to adapt [hlohmann json patches](https://github.com/nlohmann/json#json-pointer-and-json-patch)
    into something like [jsondiffpatch's deltas](https://github.com/benjamine/jsondiffpatch/blob/master/docs/deltas.md#text-diffs),
    for unified handling of state diffs involving long text strings (like code strings).

### C++ extensions

* [range-v3](https://github.com/ericniebler/range-v3), since ranges are only partially supported in Clang 13.x

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

#### Non-forked submodules

Most submodules are not forked.
Here is my process for updating to the tip of all the submodule branches:

```sh
$ git submodule update --remote
$ git add .
```

#### Forked submodules

The following modules are [forked by me](https://github.com/khiner?tab=repositories&q=&type=fork), along with the upstream branch the fork is based on:
* `imgui:docking`
* `implot:master`
* `libsoundio:master`
* `zep:master`

I like to keep my changes rebased on top of the original repo branches.
Here's my process:

```sh
$ cd lib/{library}
$ git pull --rebase upstream {branch} # `upstream` points to the original repo. See list above for the tracked branch
$ ... # Resolve any conflicts & test
$ git push --force
```

A notable exception is my zep fork, which has so many changes that almost no upstream commits will rebase successfully.
The way I handle rebasing against zep is to rebase one commit at a time, using `--strategy-option theirs` (`-Xtheirs`), and then manually verifying & porting what the merge missed:

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
Each type of FlowGrid project file is saved as [MessagePack-encoded JSON](https://github.com/nlohmann/json#binary-formats-bson-cbor-messagepack-ubjson-and-bjdata).

* `.fgs`: _FlowGrid**State**_
  - The full application state.
    An `.fgs` file contains a JSON blob with all the information needed to get back to the saved project state.
    Loading a `.fgs` project file will completely replace the application state with its own.
  - As a special case, the project file `./internal/empty.fgs` (relative to the project build folder) is used internally to load projects.
    This `empty.fgs` file is used internally to implement the `open_empty_project` action, which can be triggered via the `File->New project` menu item, or with `Cmd+n`.
    FlowGrid (over-)writes this file every launch, after initializing to empty-project values (and, currently, rendering two frames to let ImGui fully establish its context).
    This approach provides a pretty strong guarantee that if state-loading is implemented correctly, loading a new project will always produce the same, valid empty-project state.
* `.fgd`: _FlowGrid**Diffs**_
  - Instead of saving the full application state, `.fgd` project files store a JSON object with two properties:
    - `diffs`: A list of _project state diffs_ (deltas, patches), in [JSON Patch](https://jsonpatch.com/) format, corresponding to the application-state effects of every action that has effected the application's state since its launch
    - `position`: The project's position in the list of diffs. (Or, equivalently, action position or position in the undo-stack).
      When you save your project as an `.fgd` file, your current undo-position is stored here.
  - FlowGrid loads `.fgd` project files by:
    * Running the `open_empty_project` action (explained above) to clear the current application and load a fresh empty one
    * Applying each diff (patch) in the `diff` list to the application state
    * Setting the application's undo position to the non-negative integer stored in `position`
* `.fga`: _FlowGrid**Actions**_ (still working on this)
  - Finally, FlowGrid can save and load projects as a list of _actions_.
    This file stores an ordered record of _every action_ that affected the app state up to the time it was saved.
    Each item in an `.fga` file contains all the information needed to carry out a logical action affecting the app state.
    Contrast this with `.fgd` project files, which store the _results_ of each action over the application session.
    An action list item in a `.fga` file tells you, in application-domain semantics, "what happened".
    A diff item in a `.fgd` file tells you "what changed as a result of some action".

TODO: Tradeoffs between project types

## License

This software is distributed under the [GPL v3 License](./LICENSE).

GPL v3 is a strong copyleft license, which basically means any copy or modification of the code in this repo (excluding any libraries in the `lib` directory with different licenses) must also be released under the GPL v3 license.

### Why copyleft?

The audio world has lots of high quality open-source code, educational resources, libraries, and other software resources.
However, the commercial audio industry is also full of protected IP.
This is a necessary strategy for the too-few companies that manage to achieve some level of financial independence in the audio software industry, in which it's notoriously hard to do so.

Choosing a permissive license allowing for closed-source commercial usage stands to benefit more end users (musicians, artists, creators) in the short-term, since companies producing closed-source software could freely put the code right into their products.

In my experience, it's very easy, and always getting easier, to find the music software or hardware you need, while it's much harder to find the right tools and methods for creating new audio software.

Although this project is first and foremost a creative tool, the intention and spirit is much more about hacking, learning, education and research than it is about creating end media products.
For these purposes, it's more important to keep the information open than to make the functionality freely and widely available.
