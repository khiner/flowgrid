#pragma once

// This is an audio graph node that uses Faust to generate audio,
// not to be confused with Faust's own graph node (in `FaustGraph.h`)

#include "App/Audio/Graph/AudioGraphNode.h"

struct FaustNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;
    void DoInit(ma_node_graph *) const override;
    void DoUpdate() const override;
    void DoUninit() const override;
    bool NeedsRestart() const override;
};
