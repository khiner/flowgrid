#pragma once

#include "Core/Primitive/Enum.h"
#include "Project/Audio/Graph/AudioGraphNode.h"

struct ma_waveform_node;

struct WaveformNode : AudioGraphNode {
    WaveformNode(ComponentArgs &&);
    ~WaveformNode();

    void OnFieldChanged() override;
    void OnSampleRateChanged() override;

    Prop(Float, Frequency, 440.0, 20.0, 16000.0);
    Prop(Enum, Type, {"Sine", "Square", "Triangle", "Sawtooth"}, 0);
    // Amplitude is controlled by node output level.

private:
    std::unique_ptr<ma_waveform_node> _Node;

    void Render() const override;
};
