#include "FaustNode.h"

#include "Audio/Graph/AudioGraph.h"

#include "Faust.h"

#include "Audio/Graph/ma_faust_node/ma_faust_node.h"

struct FaustMaNode : MaNode, Component, ChangeListener {
    FaustMaNode(ComponentArgs &&args, AudioGraph *graph, ID dsp_id = 0)
        : MaNode(), Component(std::move(args)), Graph(graph), ParentNode(static_cast<AudioGraphNode *>(Parent)) {
        if (dsp_id != 0 && DspId == 0u) DspId.Set_(_S, dsp_id);
        Init(Graph->GetFaustDsp(DspId), Graph->SampleRate);
        DspId.RegisterChangeListener(this);
    }
    ~FaustMaNode() {
        UnregisterChangeListener(this);
        Uninit();
    }

    void OnComponentChanged() override {
        if (DspId.IsChanged()) UpdateDsp();
    }

    void Init(dsp *dsp, u32 sample_rate) {
        auto config = ma_faust_node_config_init(dsp, sample_rate, Graph->GetBufferFrames());
        ma_result result = ma_faust_node_init(Graph->Get(), &config, nullptr, &_Node);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust audio graph node: {}", int(result)));

        Node = &_Node;
    }
    void Uninit() {
        ma_faust_node_uninit(&_Node, nullptr);
    }

    void UpdateDsp() {
        auto *new_dsp = Graph->GetFaustDsp(DspId);
        auto *current_dsp = ma_faust_node_get_dsp(&_Node);
        if (!new_dsp && !current_dsp) return;

        auto new_in_channels = ma_faust_dsp_get_in_channels(new_dsp);
        auto new_out_channels = ma_faust_dsp_get_out_channels(new_dsp);
        auto current_in_channels = ma_faust_node_get_in_channels(&_Node);
        auto current_out_channels = ma_faust_node_get_out_channels(&_Node);
        if ((new_dsp && !current_dsp) || (!new_dsp && current_dsp) || current_in_channels != new_in_channels || current_out_channels != new_out_channels) {
            Uninit();
            Init(new_dsp, ma_faust_node_get_sample_rate(&_Node));
            ParentNode->NotifyConnectionsChanged();
        } else {
            ma_faust_node_set_dsp(&_Node, new_dsp);
        }
    }

    void SetDsp(TransientStore &s, ID dsp_id) {
        DspId.Set_(s, dsp_id);
        UpdateDsp();
    }

    AudioGraph *Graph;
    AudioGraphNode *ParentNode; // Type-casted parent, for convenience.

    Prop(UInt, DspId);

    ma_faust_node _Node;
};

FaustNode::FaustNode(ComponentArgs &&args, ID dsp_id) : AudioGraphNode(std::move(args), [this, dsp_id] { return CreateNode(dsp_id); }) {}

std::unique_ptr<MaNode> FaustNode::CreateNode(ID dsp_id) { return std::make_unique<FaustMaNode>(ComponentArgs{this, "Node"}, Graph, dsp_id); }

void FaustNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_faust_node_set_sample_rate((ma_faust_node *)Get(), Graph->SampleRate);
}

ID FaustNode::GetDspId() const { return reinterpret_cast<FaustMaNode *>(Node.get())->DspId; }
void FaustNode::SetDsp(TransientStore &s, ID id) { reinterpret_cast<FaustMaNode *>(Node.get())->SetDsp(s, id); }
