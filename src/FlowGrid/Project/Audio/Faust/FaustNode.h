#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's graph UI nodes (in `FaustGraphs.h`).

#include "Project/Audio/Graph/AudioGraphNode.h"

class dsp;

struct FaustNode : AudioGraphNode {
    FaustNode(ComponentArgs &&);

    void OnSampleRateChanged() override;

    void SetDsp(ID, dsp *);

    Prop(UInt, DspId);

private:
    std::unique_ptr<MaNode> CreateNode() const;
};
