#pragma once

/**
`FaustDSP` is a wrapper around Faust DSP/Box instances.
`Audio.Faust` listens to `Audio.Faust.Code` changes and updates `FaustDSP` instances accordingly.

Components that listen to `FaustDsp` changes:
- `Audio.Faust.FaustGraph`: A highly configurable, live-updating block diagram of the Faust DSP.
  - By default, `FaustGraph` matches the FlowGrid style (which is ImGui's dark style).
    But it can be configured to exactly match the Faust SVG diagram style.
    `FaustGraph` can also be rendered as an SVG diagram.
    `FaustGraph.Style` should be the same as the one produced by `faust2svg` with the same DSP code (at least visually!)
- `Audio.Faust.Params`: Interface for the Faust DSP params. TODO: Not undoable yet.
- `Audio.Graph.Nodes.Faust`: Updates the MiniAudio node and graph connections to reflect the new DSP.

Here is the chain of notifications/updates in response to a Faust DSP code change:
```
Audio.Faust.Code
    -> Audio.Faust
        -> Audio.Faust.FaustDSP
            -> Audio.Faust.FaustGraph
            -> Audio.Faust.FaustParams
            -> Audio.Graph.Nodes.Faust
```
**/

class dsp;

struct FaustDspChangeListener {
    virtual void OnFaustDspChanged(dsp *) = 0;
};
