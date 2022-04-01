# FlowGrid

Prototyping the new stack for FlowGrid.

I'm still actively building this. Currently, I'm basically trying to maximally mash together some wonderful libraries (
see [**Stack**](#stack)):

## Features

* Faust live code editor
    - Currently, a very basic `ImGui::InputTextMultiline`
* All user actions are put into a full undo stack
    - An invariant of the application is that its state can always be fully reconstructed from diffs in the undo stack.
* Toggle audio mute
* Toggle audio thread running
* Choose background color
* Toggle the ImGui demo window

## Stack

### Audio

* Faust for DSP
* libsoundio for audio backend

### UI/UX

* ImGui for UI
* [zep](https://github.com/Rezonality/zep) for Faust code editing

### Backend

json, ConcurrentQueue, diff-match-patch-cpp-stl

## System requirements

### Mac

```shell
$ git submodule update --recursive --remote
$ brew install pkgconfig llvm freetype
$ brew link llvm --force # I needed this to put the `llvm-config` executable in my PATH
```

TODO: Will probably want to build llvm locally as a submodule, and point to it.
See [TD-Faust](https://github.com/DBraun/TD-Faust/blob/02f35e4343370559c779468413c32179f55c6552/build_macos.sh#L5-L31)
as an example.

TODO https://github.com/wolfpld/tracy
