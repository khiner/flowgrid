#pragma once

#include "Audio/Graph/AudioGraphNode.h"

struct WaveformNode : AudioGraphNode {
    WaveformNode(ComponentArgs &&);

    void OnComponentChanged() override;
    void OnSampleRateChanged() override;

    Prop(Float, Frequency, 440.0, 20.0, 16000.0);
    Prop(Enum, Type, {"Sine", "Square", "Triangle", "Sawtooth"}, 0);
    // Amplitude is controlled by node output level.

private:
    std::unique_ptr<MaNode> CreateNode() const;

    void Render() const override;

    void UpdateFrequency();
    void UpdateType();
};
