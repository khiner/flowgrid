#include "FaustNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "Faust.h"

#include "Project/Audio/Graph/ma_faust_node/ma_faust_node.h"

struct FaustMaNode : MaNode, Component {
    FaustMaNode(ComponentArgs &&args, AudioGraph *graph, ID dsp_id = 0) : MaNode(), Component(std::move(args)), Graph(graph) {
        if (dsp_id != 0 && DspId == 0u) DspId.Set_(dsp_id);
        auto *dsp = Graph->GetFaustDsp(DspId);
        if (dsp == nullptr) throw std::runtime_error("Attempting to create a Faust node with a null DSP.");
        Init(dsp, Graph->SampleRate);
    }
    ~FaustMaNode() {
        Uninit();
    }

    void Init(dsp *dsp, u32 sample_rate) {
        auto config = ma_faust_node_config_init(dsp, sample_rate);
        ma_result result = ma_faust_node_init(Graph->Get(), &config, nullptr, &_Node);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust audio graph node: {}", int(result)));
        Node = &_Node;
    }
    void Uninit() {
        ma_faust_node_uninit(&_Node, nullptr);
    }

    // Returns true if the node was reinitialized.
    bool SetDsp(ID id, dsp *dsp) {
        DspId.Set_(id);
        if (ma_faust_node_get_out_channels(&_Node) != ma_faust_dsp_get_out_channels(dsp) || ma_faust_node_get_in_channels(&_Node) != ma_faust_dsp_get_in_channels(dsp)) {
            Uninit();
            Init(dsp, ma_faust_node_get_sample_rate(&_Node));
            return true;
        } else {
            ma_faust_node_set_dsp(&_Node, dsp);
            return false;
        }
    }

    AudioGraph *Graph{nullptr};

    Prop(UInt, DspId);

    ma_faust_node _Node;
};

// todo destroy node when dsp is null
FaustNode::FaustNode(ComponentArgs &&args, ID dsp_id) : AudioGraphNode(std::move(args), [this, dsp_id] { return CreateNode(dsp_id); }) {}

std::unique_ptr<MaNode> FaustNode::CreateNode(ID dsp_id) {
    return std::make_unique<FaustMaNode>(ComponentArgs{this, "Node"}, Graph, dsp_id);
}

void FaustNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_faust_node_set_sample_rate((ma_faust_node *)Get(), Graph->SampleRate);
}

void FaustNode::SetDsp(ID id, dsp *dsp) {
    const bool was_reinitialized = reinterpret_cast<FaustMaNode *>(Node.get())->SetDsp(id, dsp);
    if (was_reinitialized) NotifyConnectionsChanged();
}
