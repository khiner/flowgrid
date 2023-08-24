#pragma once

/**
Components that listen to `FaustDSP` changes:
- `Audio.Faust.FaustGraphs` (listens to Box): Extensively configurable, live-updating block diagrams for all Faust DSP instances.
  - By default, `FaustGraph` matches the FlowGrid style (which is ImGui's dark style), but it can be configured to exactly match the Faust SVG diagram style.
    `FaustGraph` can also be rendered as an SVG diagram.
    When the graph style is set to the 'Faust' preset, it should look the same as the one produced by `faust2svg` with the same DSP code.
- `Audio.Faust.Params` (listens to DSP): Interfaces for the params for each Faust DSP instance. TODO: Not undoable yet.
- `Audio.Faust.Logs` (listens to FaustDSP, accesses error messages): A window to display Faust compilation errors.

Here is the chain of notifications/updates in response to a Faust DSP code change:
```
Audio.Faust.FaustDsp.Code -> Audio.Faust.FaustDsp
    -> Audio.Faust.FaustGraphs
    -> Audio.Faust.FaustParams
    -> Audio.Faust.FaustLogs
    -> Audio
        -> Audio.Graph.Nodes.Faust
```
**/

using ID = unsigned int;

class CTree;
typedef CTree *Box;
struct FaustBoxChangeListener {
    virtual void OnFaustBoxChanged(ID, Box) = 0;
    virtual void OnFaustBoxAdded(ID, Box) = 0;
    virtual void OnFaustBoxRemoved(ID) = 0;
};

class dsp;
struct FaustDspChangeListener {
    virtual void OnFaustDspChanged(ID, dsp *) = 0;
    virtual void OnFaustDspAdded(ID, dsp *) = 0;
    virtual void OnFaustDspRemoved(ID) = 0;
};

struct FaustDSP;
struct FaustChangeListener {
    virtual void OnFaustChanged(ID, const FaustDSP &) = 0;
    virtual void OnFaustAdded(ID, const FaustDSP &) = 0;
    virtual void OnFaustRemoved(ID) = 0;
};

enum NotificationType {
    Changed,
    Added,
    Removed
};

struct FaustDSPContainer {
    virtual void NotifyListeners(NotificationType, const FaustDSP &) const = 0;
    virtual void NotifyBoxListeners(NotificationType, const FaustDSP &) const = 0;
    virtual void NotifyDspListeners(NotificationType, const FaustDSP &) const = 0;
};
