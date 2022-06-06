# FlowGrid

Prototyping the new stack for FlowGrid.

(Old version [here](https://github.com/khiner/flowgrid)) with an out-of-date demo gif and in general dissaray.
Planning on making that solid in what it does soon (without adding any new features), but got carried away having fun
laying foundations for a new [**Stack**](#stack), aimed at helping me be more productive.

Still actively building this.

Focusing here on making it easier to stay in flow state while using _and creating_ the app.

* Keeping full clean-and-build times low
* Keeping rebuild times low after edits of commonly imported headers
* Spend more time upfront trying to get simple and flexible foundations right
* Always Be Improving the experience of creating the application
* Have fun building the app. This will help me actually build it.
* Let myself optimize. More Fast <=> More Fun. In computers, fast things are good and always more fun than slow things.

So far, I'm basically trying to mash together some great libraries (see [**Stack**](#stack)):

## Build and run

### Mac

#### System requirements

Prepare your environment:

```sh
$ git clone --recursive git@github.com:khiner/flowgrid2.git
$ brew install cmake pkgconfig llvm freetype
$ brew link llvm --force
```

#### IDE clean/build/run

* **CLion:** I use CLion to develop this application, so that's the only IDE I can attest to working smoothly.
* **TODO:** anything needed for a VSCode project? would be nice to be able to assess it since most refactoring seems
  weirdly slow in CLion, and doesn't support many features in other JetBrains products.

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
    * For now just moved this action processing work to the UI thread to avoid issues with concurrent reads/writes to
      complex structures like JSON)
* ~~[diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl) for diff-patching on unstructured
  text~~
    - Was using to handle ImGui `.ini` settings string diffs, but those are now deserialized into the structured state.
      Will be using this again soon, to
      adapt [hlohmann json patches](https://github.com/nlohmann/json#json-pointer-and-json-patch) into something
      like [jsondiffpatch's deltas](https://github.com/benjamine/jsondiffpatch/blob/master/docs/deltas.md#text-diffs),
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

The following modules are [forked by me](https://github.com/khiner?tab=repositories&q=&type=fork), along with the
upstream branch the fork is based on:

* `imgui:docking`
* `implot:master`
* `ImGuiFileDialog:master`
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
The way I handle rebasing against zep is to rebase one commit at a time, using `--strategy-option theirs` (`-Xtheirs`),
and then manually verifying & porting what the merge missed:

```sh
$ cd lib/zep
$ git pull --rebase -Xtheirs upstream {commit_sha}
$ ... # Resolve any conflicts, port any missing changes manually, verify...
$ git push --force
```

## Application docs

### Project files

FlowGrid project files
are [MessagePack-encoded JSON](https://github.com/nlohmann/json#binary-formats-bson-cbor-messagepack-ubjson-and-bjdata).
Project files are saved with a `.flo` extension.

## License

This software is distributed under the [GPL v3 License](./LICENSE).

GPL v3 is a strong copyleft license, which basically means any copy or modification of the code in this repo (excluding
any libraries in the `lib` directory with different licenses) must also
be released under the GPL v3 license.

### Why copyleft?

The audio world has a ton of amazing and fully open-source code, educational resources, libraries, etc.
However, the commercial audio industry is also full of protected IP.
This is a necessary strategy for the too-few companies that manage to achieve some level of financial independence in an
industry in which it's notoriously difficult to do so.

Choosing a permissive license that allows for closed-source commercial usage stands to benefit more end users (
musicians, artists, creators) in the short-term, since companies producing closed-source software could freely put the
code right into their products.

In my experience, it's very easy, and always getting easier, to find the software/hardware you need to create music,
while it's much harder to find an effective path to creating new audio software.

Ultimately, although this project is first and foremost a creative tool, the intention and spirit is much more about
hacking, learning, education and research than it is about creating end media products.
For these purposes, it's more important to keep the information open than to make the functionality freely and widely
available.
