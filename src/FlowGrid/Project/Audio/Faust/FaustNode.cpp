#include "FaustNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "Faust.h"

#include "Project/Audio/Graph/ma_faust_node/ma_faust_node.h"

struct FaustMaNode : MaNode {
    FaustMaNode(ma_node_graph *graph, dsp *dsp, u32 sample_rate) {
        auto config = ma_faust_node_config_init(dsp, sample_rate);
        ma_result result = ma_faust_node_init(graph, &config, nullptr, &_Node);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust audio graph node: {}", int(result)));
        Node = &_Node;
    }
    ~FaustMaNode() {
        ma_faust_node_uninit(&_Node, nullptr);
    }

    ma_faust_node _Node;
};

// todo destroy node when dsp is null
FaustNode::FaustNode(ComponentArgs &&args) : AudioGraphNode(std::move(args), [this] { return CreateNode(); }) {}

std::unique_ptr<MaNode> FaustNode::CreateNode() const {
    return std::make_unique<FaustMaNode>(Graph->Get(), Graph->GetFaustDsp(), Graph->SampleRate);
}

void FaustNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_faust_node_set_sample_rate((ma_faust_node *)Get(), Graph->SampleRate);
}
