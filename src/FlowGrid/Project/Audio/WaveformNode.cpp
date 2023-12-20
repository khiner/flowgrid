#include "WaveformNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "Graph/ma_waveform_node/ma_waveform_node.h"

#include "imgui.h"

struct WaveformMaNode : MaNode {
    WaveformMaNode(ma_node_graph *graph, u32 sample_rate, ma_waveform_type type, float frequency) {
        auto config = ma_waveform_node_config_init(sample_rate, type, frequency);
        ma_result result = ma_waveform_node_init(graph, &config, nullptr, &_Node);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the waveform node: {}", int(result)));
        Node = &_Node;
    }
    ~WaveformMaNode() {
        ma_waveform_node_uninit(&_Node, nullptr);
    }

    ma_waveform_node _Node;
};

WaveformNode::WaveformNode(ComponentArgs &&args) : AudioGraphNode(std::move(args), [this] { return CreateNode(); }) {
    UpdateFrequency();
    UpdateType();
    Frequency.RegisterChangeListener(this);
    Type.RegisterChangeListener(this);
}

std::unique_ptr<MaNode> WaveformNode::CreateNode() const {
    return std::make_unique<WaveformMaNode>(Graph->Get(), Graph->SampleRate, ma_waveform_type(int(Type)), Frequency);
}

void WaveformNode::UpdateFrequency() {
    ma_waveform_set_frequency(&((ma_waveform_node *)Get())->waveform, Frequency);
}

void WaveformNode::UpdateType() {
    ma_waveform_set_type(&((ma_waveform_node *)Get())->waveform, ma_waveform_type(int(Type)));
}

void WaveformNode::OnComponentChanged() {
    AudioGraphNode::OnComponentChanged();

    if (Frequency.IsChanged()) UpdateFrequency();
    if (Type.IsChanged()) UpdateType();
}

void WaveformNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_waveform_node_set_sample_rate(((ma_waveform_node *)Get()), Graph->SampleRate);
}

void WaveformNode::Render() const {
    Frequency.Draw();
    Type.Draw();
    ImGui::Spacing();
    AudioGraphNode::Render();
}
