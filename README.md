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
