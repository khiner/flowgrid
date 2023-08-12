#pragma once

#include "FaustBox.h"

/**
```
Audio.Faust.Code ->
    FaustDSP ->
        FaustDspChangeListener ->
            - Audio.Faust.FaustGraph
            - Audio.Faust.FaustParams
            - Audio.Graph.Nodes.Faust

`FaustDsp` is a wrapper around the Faust DSP/Box objects.
`FaustDsp` listens to `Audio.Faust.Code` changes and creates/rebuilds/destroys the Faust DSP and box instances accordingly.
Components that listen to `FaustDsp` changes:
- `Audio.Faust.FaustGraph`: An extensively configurable, live-updating block diagram of the Faust DSP.
  - By default, `FaustGraph` matches the FlowGrid style (which is ImGui's dark style).
    But it can be configured to exactly match the Faust SVG diagram style.
    `FaustGraph` can also be rendered as an SVG diagram.
    When `FaustGraph.Style. hould be the same as the one produced by `faust2svg` with the same DSP code (at least visually!)
- `Audio.Faust.Params`: Interface for the Faust DSP params. TODO: Not undoable yet.
- `Audio.Graph.Nodes.Faust`: Updates the MiniAudio node and graph connections to reflect the new DSP.
**/

class box;

struct FaustBoxChangeListener {
    virtual void OnFaustBoxChanged(Box) = 0;
};
