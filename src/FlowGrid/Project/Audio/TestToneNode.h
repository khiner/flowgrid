#pragma once

#include "Project/Audio/Graph/AudioGraphNode.h"

struct TestToneNode : AudioGraphNode {
    TestToneNode(ComponentArgs &&);

    void OnFieldChanged() override;

private:
    ma_node *DoInit() override;
};
