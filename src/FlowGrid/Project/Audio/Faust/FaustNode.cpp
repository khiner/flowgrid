#include "FaustNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "Faust.h"

#include "Project/Audio/Graph/ma_faust_node/ma_faust_node.h"

struct FaustMaNode : MaNode {
    FaustMaNode(ma_node_graph *graph, dsp *dsp, u32 sample_rate) {
        if (dsp == nullptr) throw std::runtime_error("Attempting to create a Faust node with a null DSP.");

        Init(graph, dsp, sample_rate);
    }
    ~FaustMaNode() {
        Uninit();
    }

    void Init(ma_node_graph *graph, dsp *dsp, u32 sample_rate) {
        auto config = ma_faust_node_config_init(dsp, sample_rate);
        ma_result result = ma_faust_node_init(graph, &config, nullptr, &_Node);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust audio graph node: {}", int(result)));
        Node = &_Node;
    }
    void Uninit() {
        ma_faust_node_uninit(&_Node, nullptr);
    }

    // Returns true if the node was reinitialized.
    bool SetDsp(dsp *dsp) {
        if (ma_faust_node_get_out_channels(&_Node) != ma_faust_dsp_get_out_channels(dsp) || ma_faust_node_get_in_channels(&_Node) != ma_faust_dsp_get_in_channels(dsp)) {
            Uninit();
            Init(_Node.base.pNodeGraph, dsp, ma_faust_node_get_sample_rate(&_Node));
            return true;
        } else {
            ma_faust_node_set_dsp(&_Node, dsp);
            return false;
        }
    }

    ma_faust_node _Node;
};

// todo destroy node when dsp is null
FaustNode::FaustNode(ComponentArgs &&args) : AudioGraphNode(std::move(args), [this] { return CreateNode(); }) {}

std::unique_ptr<MaNode> FaustNode::CreateNode() const {
    return std::make_unique<FaustMaNode>(Graph->Get(), Graph->GetFaustDsp(DspId), Graph->SampleRate);
}

void FaustNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_faust_node_set_sample_rate((ma_faust_node *)Get(), Graph->SampleRate);
}

void FaustNode::SetDsp(ID id, dsp *dsp) {
    DspId.Set_(id);
    const bool was_reinitialized = reinterpret_cast<FaustMaNode *>(Node.get())->SetDsp(dsp);
    if (was_reinitialized) NotifyConnectionsChanged();
}
