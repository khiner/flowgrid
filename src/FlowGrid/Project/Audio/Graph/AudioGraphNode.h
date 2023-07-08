#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"

// xxx miniaudio should not be in a header.
// Only needed here singe forward-declaring `ma_splitter_node` is not working for unique_ptr and I don't know why.
#include "miniaudio.h"

#include "ma_monitor_node/ma_monitor_node.h"

struct AudioGraph;
using ma_node = void;

// using ma_node = void;
// struct ma_splitter_node;

// Corresponds to `ma_node`.
// This base `Node` can either be specialized or instantiated on its own.
struct AudioGraphNode : Component, Field::ChangeListener {
    AudioGraphNode(ComponentArgs &&);
    virtual ~AudioGraphNode();

    void OnFieldChanged() override;
    void Set(ma_node *);

    ma_node *Node;

    Count InputBusCount() const;
    Count OutputBusCount() const;
    Count InputChannelCount(Count bus) const;
    Count OutputChannelCount(Count bus) const;

    bool IsSource() const { return OutputBusCount() > 0 && Name != "Output"; }
    bool IsDestination() const { return InputBusCount() > 0; }

    void ConnectTo(const AudioGraphNode &);
    void DisconnectOutputs();

    void Init();
    void Update();
    void Uninit();

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.", true);
    Prop_(Bool, Muted, "?Mute the node. This does not affect CPU load.", false);
    Prop(Float, Volume, 1.0);
    Prop(Bool, MonitorOutput, false);

    struct SplitterDeleter {
        void operator()(ma_splitter_node *);
    };
    std::vector<std::unique_ptr<ma_splitter_node, SplitterDeleter>> SplitterNodes;

    struct MonitorDeleter {
        void operator()(ma_monitor_node *);
    };
    std::unique_ptr<ma_monitor_node, MonitorDeleter> OutputMonitorNode;

protected:
    void Render() const override;

    virtual ma_node *DoInit() { return nullptr; };
    virtual void DoUninit() {}

    void UpdateVolume();
    void UpdateMonitors();

    const AudioGraph *Graph;
};
