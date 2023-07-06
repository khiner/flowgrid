#pragma once

#include "Project/Audio/Graph/AudioGraphNode.h"
#include "Core/Primitive/Enum.h"

struct TestToneNode : AudioGraphNode {
    TestToneNode(ComponentArgs &&);

    void OnFieldChanged() override;

    Prop(Float, Amplitude, 1.0);
    Prop(Float, Frequency, 440.0, 20.0, 16000.0);
    Prop(Enum, Type, {"Sine", "Square", "Triangle", "Sawtooth"}, 0);

private:
    void Render() const override;

    ma_node *DoInit() override;
    void DoUninit() override;
};
