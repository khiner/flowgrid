#pragma once

// An audio graph node that uses Faust to generate audio, not to be confused with Faust's own graph node (in `FaustGraph.h`).

#include "Project/Audio/Graph/AudioGraphNode.h"

struct FaustNode : AudioGraphNode {
    FaustNode(ComponentArgs &&);

    void OnSampleRateChanged() override;

private:
    std::unique_ptr<MaNode> CreateNode() const;
};
