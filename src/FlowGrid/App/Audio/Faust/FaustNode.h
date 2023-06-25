#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "App/Audio/Graph/AudioGraphNode.h"

struct dsp;

struct FaustNode : AudioGraphNode, Field::ChangeListener {
    FaustNode(ComponentArgs &&, bool on = true);
    ~FaustNode();

    bool NeedsRestart() const override;
    void OnFieldChanged() override {}

private:
    void DoInit(ma_node_graph *) override;
    void DoUpdate() override;
    void DoUninit() override;

    // This node class currently owns the Faust DSP instance, but there are a couple of issues with this:
    //   - We assume everywhere that there's only one Faust DSP instance, but it would be better to support multiple.
    //   - Should be able to manage a Faust (dsp) instance separately from a node, for instance to render a Faust debug metrics window.
    //     - Then `FaustNode`, `FaustGraph`, and `FaustParams` would depend on a `Faust` instance.
    //   - The current `On..Change` signaling method pattern is bad.
    dsp *Dsp;

    void InitDsp();
    void UninitDsp();
    void UpdateDsp();
};
