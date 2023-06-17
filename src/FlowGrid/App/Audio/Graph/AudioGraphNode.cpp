#include "AudioGraphNode.h"

#include "miniaudio.h"

AudioGraphNode::AudioGraphNode(ComponentArgs &&args, bool on) : Component(std::move(args)) {
    store::Set(On, on);
}

void *AudioGraphNode::Get() const {
    auto it = DataForId.find(Id);
    return it != DataForId.end() ? it->second : nullptr;
}

void AudioGraphNode::Set(ma_node *data) {
    if (data == nullptr) DataForId.erase(Id);
    else DataForId[Id] = data;
}

Count AudioGraphNode::InputBusCount() const { return ma_node_get_input_bus_count(Get()); }
Count AudioGraphNode::OutputBusCount() const { return ma_node_get_output_bus_count(Get()); }
Count AudioGraphNode::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Get(), bus); }
Count AudioGraphNode::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Get(), bus); }

void AudioGraphNode::Init(ma_node_graph *graph) {
    DoInit(graph);
    NeedsRestart(); // xxx Updates cached values as side effect.
}

void AudioGraphNode::Update(ma_node_graph *graph) {
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

void AudioGraphNode::Uninit() {
    if (!Get()) return;

    DoUninit();
    Set(nullptr);
}

void AudioGraphNode::DoUninit() {
    ma_node_uninit(Get(), nullptr);
}

void AudioGraphNode::Render() const {
    On.Draw();
    Volume.Draw();
}
