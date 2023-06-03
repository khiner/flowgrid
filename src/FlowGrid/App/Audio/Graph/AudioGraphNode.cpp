#include "AudioGraphNode.h"

#include "miniaudio.h"

AudioGraphNode::AudioGraphNode(Stateful::Base *parent, string_view path_segment, string_view name_help, bool on)
    : UIStateful(parent, path_segment, name_help) {
    store::Set(On, on);
}

void *AudioGraphNode::Get() const { return DataFor.contains(Id) ? DataFor.at(Id) : nullptr; }
void AudioGraphNode::Set(ma_node *data) const {
    if (data == nullptr) DataFor.erase(Id);
    else DataFor[Id] = data;
}

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Get()); }
Count AudioGraphNode::OutputBusCount() const { return ma_node_get_output_bus_count(Get()); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Get(), bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Get(), bus); }

void AudioGraphNode::Init(ma_node_graph *graph) const {
    DoInit(graph);
    NeedsRestart(); // xxx Updates cached values as side effect.
}

void AudioGraphNode::Update(ma_node_graph *graph) const {
    DoUpdate();

    const bool is_initialized = Get() != nullptr;
    const bool needs_restart = NeedsRestart(); // Don't inline! Must run during every update.
    if (On && !is_initialized) {
        Init(graph);
    } else if (!On && is_initialized) {
        Uninit();
    } else if (needs_restart && is_initialized) {
        Uninit();
        Init(graph);
    }
    if (On) ma_node_set_output_bus_volume(Get(), 0, Volume);
}
void AudioGraphNode::Uninit() const {
    if (!Get()) return;

    DoUninit();
    Set(nullptr);
}
void AudioGraphNode::DoUninit() const {
    ma_node_uninit(Get(), nullptr);
}
void AudioGraphNode::Render() const {
    On.Draw();
    Volume.Draw();
}
