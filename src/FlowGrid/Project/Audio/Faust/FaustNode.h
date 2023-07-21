#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "FaustDspChangeListener.h"
#include "Project/Audio/Graph/AudioGraphNode.h"

class dsp;

struct FaustNode : AudioGraphNode, FaustDspChangeListener {
    FaustNode(ComponentArgs &&);

    void OnFieldChanged() override;
    void OnFaustDspChanged(dsp *) override;
    void OnSampleRateChanged() override;

private:
    void Init();
};
