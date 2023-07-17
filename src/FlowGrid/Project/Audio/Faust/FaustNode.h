#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "FaustDspChangeListener.h"
#include "Project/Audio/Graph/AudioGraphNode.h"

class dsp;

struct FaustNode : AudioGraphNode, FaustDspChangeListener {
    using AudioGraphNode::AudioGraphNode;

    void OnFieldChanged() override;
    void OnFaustDspChanged(dsp *) override;
    void OnDeviceSampleRateChanged() override;

private:
    ma_node *DoInit() override;
};
