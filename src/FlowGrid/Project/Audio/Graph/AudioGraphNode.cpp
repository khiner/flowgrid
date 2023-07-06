#include "AudioGraph.h"

#include "miniaudio.h"

AudioGraphNode::AudioGraphNode(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {
    Volume.RegisterChangeListener(this);
}
AudioGraphNode::~AudioGraphNode() {
    Field::UnregisterChangeListener(this);
}

void AudioGraphNode::OnFieldChanged() {
    if (Volume.IsChanged()) UpdateVolume();
}

void AudioGraphNode::Set(ma_node *node) {
    Node = node;
}

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Node); }
Count AudioGraphNode::OutputBusCount() const { return ma_node_get_output_bus_count(Node); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Node, bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Node, bus); }

void AudioGraphNode::Init() {
    Set(DoInit());
}

void AudioGraphNode::UpdateVolume() {
    if (On) ma_node_set_output_bus_volume(Node, 0, Volume);
}

void AudioGraphNode::Update() {
    const bool is_initialized = Node != nullptr;
    if (On && !is_initialized) Init();
    else if (!On && is_initialized) Uninit();

    UpdateVolume();
}

void AudioGraphNode::Uninit() {
    if (Node == nullptr) return;

    DoUninit();
    ma_node_uninit(Node, nullptr);
    Set(nullptr);
}

void AudioGraphNode::Render() const {
    On.Draw();
    Volume.Draw();
}
