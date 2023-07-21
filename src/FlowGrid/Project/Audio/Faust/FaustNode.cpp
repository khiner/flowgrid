#include "FaustNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "Faust.h"

#include "Project/Audio/Graph/ma_faust_node/ma_faust_node.h"

// todo destroy node when dsp is null
FaustNode::FaustNode(ComponentArgs &&args, dsp *dsp) : AudioGraphNode(std::move(args)) {
    auto config = ma_faust_node_config_init(dsp, GetSampleRate());
    _Node = std::make_unique<ma_faust_node>();
    int result = ma_faust_node_init(Graph->Get(), &config, nullptr, _Node.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust audio graph node: {}", result));
    Node = _Node.get();

    UpdateAll();
}

FaustNode::~FaustNode() {
    ma_faust_node_uninit(_Node.get(), nullptr);
    Node = nullptr;
}

void FaustNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_faust_node_set_sample_rate(_Node.get(), GetSampleRate());
}
