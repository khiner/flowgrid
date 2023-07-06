#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "Project/Audio/Graph/AudioGraphNode.h"

class dsp;

struct FaustNode : AudioGraphNode {
    FaustNode(ComponentArgs &&);

    void OnFieldChanged() override;
    void OnDspChanged(dsp *);

private:
    ma_node *DoInit() override;
};
