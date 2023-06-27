#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "App/Audio/Graph/AudioGraphNode.h"

struct dsp;

struct FaustNode : AudioGraphNode, Field::ChangeListener {
    FaustNode(ComponentArgs &&, bool on = true);
    ~FaustNode();

    void OnFieldChanged() override {}

    void InitDsp();
    void UninitDsp();

    dsp *Dsp;

private:
    void DoInit(ma_node_graph *) override;
};
