# FlowGrid

Prototyping the new stack for FlowGrid.

Currently:

* `libsoundio` audio loop running on its own thread
* Simple `imgui` interface to control:
    - application background color
    - turning a sine wave on and off
    - changing sine frequency and amplitude
    - shutting down audio loop entirely
    - opening the `imgui` demo window
* Single global application state object `state` of type `State`
    - all state transformations happen via `action`s
    - TODO: undo/redo using state diffs
* TODO: Faust for audio backend
    - TODO: faust text editor, recompiling Faust graph every time an 'Apply' button is pressed

## System requirements

### Mac

```shell
$ git submodule update --recursive --remote
$ brew install pkgconfig llvm
$ brew link llvm --force # I needed this to put the `llvm-config` executable in my PATH
```

TODO: Will probably want to build llvm locally as a submodule, and point to it.
See [TD-Faust](https://github.com/DBraun/TD-Faust/blob/02f35e4343370559c779468413c32179f55c6552/build_macos.sh#L5-L31)
as an example.
