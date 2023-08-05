#include "WaveformNode.h"

#include "Project/Audio/Graph/AudioGraph.h"

#include "Graph/ma_waveform_node/ma_waveform_node.h"

#include "imgui.h"

WaveformNode::WaveformNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Frequency.RegisterChangeListener(this);
    Type.RegisterChangeListener(this);

    auto config = ma_waveform_node_config_init(Graph->SampleRate, ma_waveform_type(int(Type)), Frequency);
    _Node = std::make_unique<ma_waveform_node>();
    ma_result result = ma_waveform_node_init(Graph->Get(), &config, nullptr, _Node.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the waveform node: {}", int(result)));
    Node = _Node.get();

    UpdateAll();
}

WaveformNode::~WaveformNode() {
    ma_waveform_node_uninit(_Node.get(), nullptr);
}

void WaveformNode::UpdateAll() {
    AudioGraphNode::UpdateAll();
    UpdateFrequency();
    UpdateType();
}

void WaveformNode::UpdateFrequency() {
    ma_waveform_set_frequency(&_Node.get()->waveform, Frequency);
}

void WaveformNode::UpdateType() {
    ma_waveform_set_type(&_Node.get()->waveform, ma_waveform_type(int(Type)));
}

void WaveformNode::OnFieldChanged() {
    AudioGraphNode::OnFieldChanged();

    if (Frequency.IsChanged()) UpdateFrequency();
    if (Type.IsChanged()) UpdateType();
}

void WaveformNode::OnSampleRateChanged() {
    AudioGraphNode::OnSampleRateChanged();
    ma_waveform_node_set_sample_rate(_Node.get(), Graph->SampleRate);
}

void WaveformNode::Render() const {
    Frequency.Draw();
    Type.Draw();
    ImGui::Spacing();
    AudioGraphNode::Render();
}
