#pragma once

#include "Core/Primitive/Enum.h"
#include "Project/Audio/Graph/AudioGraphNode.h"


struct TestToneNode : AudioGraphNode {
    TestToneNode(ComponentArgs &&);

    void OnFieldChanged() override;
    void OnDeviceSampleRateChanged() override;

    Prop(Float, Frequency, 440.0, 20.0, 16000.0);
    Prop(Enum, Type, {"Sine", "Square", "Triangle", "Sawtooth"}, 0);
    // Amplitude is controlled by node output level.

private:
    void Render() const override;

    ma_node *DoInit(ma_node_graph *) override;
    void DoUninit() override;
};
