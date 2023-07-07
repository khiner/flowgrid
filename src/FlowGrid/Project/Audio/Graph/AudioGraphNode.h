#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"

struct AudioGraph;

using ma_node = void;

// Corresponds to `ma_node`.
// This base `Node` can either be specialized or instantiated on its own.
struct AudioGraphNode : Component, Field::ChangeListener {
    AudioGraphNode(ComponentArgs &&);
    virtual ~AudioGraphNode();

    void OnFieldChanged() override;
    void Set(void *);

    void *Node;

    Count InputBusCount() const;
    Count OutputBusCount() const;
    Count InputChannelCount(Count bus) const;
    Count OutputChannelCount(Count bus) const;

    bool IsSource() const { return OutputBusCount() > 0 && Name != "Output"; }
    bool IsDestination() const { return InputBusCount() > 0; }

    void Init();
    void Update();
    void Uninit();

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.", true);
    Prop_(Bool, Muted, "?Mute the node. This does not affect CPU load.", false);
    Prop(Float, Volume, 1.0);

protected:
    void Render() const override;

    virtual ma_node *DoInit() { return nullptr; };
    virtual void DoUninit() {}

    void UpdateVolume();

    const AudioGraph *Graph;
};
