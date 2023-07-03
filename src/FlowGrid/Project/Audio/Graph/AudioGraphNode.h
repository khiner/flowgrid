#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"

struct AudioGraph;

// Corresponds to `ma_node_base`.
// MA tracks nodes with an `ma_node *` type, where `ma_node` is an alias to `void`.
// We don't forward-declare `ma_node` here because it prevent's miniaudio's methods from correctly deducing the MA type.
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

    bool IsSource() const { return OutputBusCount() > 0; }
    bool IsDestination() const { return InputBusCount() > 0; }

    void Init();
    void Update();
    void Uninit();

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.", true);
    Prop(Float, Volume, 1.0);

protected:
    void Render() const override;

    virtual void DoInit(){};
    virtual void DoUninit() {}

    void UpdateVolume();

    const AudioGraph *Graph;
};
