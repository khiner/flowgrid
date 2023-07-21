#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "Project/Audio/Graph/AudioGraphNode.h"

class dsp;

struct ma_faust_node;

struct FaustNode : AudioGraphNode {
    FaustNode(ComponentArgs &&, dsp *);
    ~FaustNode();

    void OnSampleRateChanged() override;

private:
    std::unique_ptr<ma_faust_node> _Node;
};
