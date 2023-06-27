#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "App/Audio/Graph/AudioGraphNode.h"

class dsp;

struct FaustNode : AudioGraphNode, Field::ChangeListener {
    FaustNode(ComponentArgs &&, bool on = true);
    ~FaustNode();

    void OnFieldChanged() override {}
    void OnDspChanged(dsp *);

private:
    void DoInit(ma_node_graph *) override;
};
