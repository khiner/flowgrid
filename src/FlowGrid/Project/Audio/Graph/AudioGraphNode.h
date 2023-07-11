#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Float.h"

#include "Project/Audio/AudioIO.h"

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
    inline Count BusCount(IO io) const { return io == IO_In ? InputBusCount() : OutputBusCount(); }

    Count InputChannelCount(Count bus) const;
    Count OutputChannelCount(Count bus) const;
    inline Count ChannelCount(IO io, Count bus) const { return io == IO_In ? InputChannelCount(bus) : OutputChannelCount(bus); }

    ma_monitor_node *GetMonitorNode(IO io) const { return io == IO_In ? InputMonitorNode.get() : OutputMonitorNode.get(); }

    bool IsOutput() const noexcept { return Name == "Output"; }

    inline ma_node *InputNode() const noexcept { return InputMonitorNode ? InputMonitorNode.get() : Node; }
    inline ma_node *OutputNode() const noexcept { return OutputMonitorNode ? OutputMonitorNode.get() : Node; }

    void ConnectTo(AudioGraphNode &);
    void DisconnectAll();

    inline void SetActive(bool is_active) noexcept { IsActive = is_active; }

    void Init();
    void Update();
    void Uninit();

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.\nIt is active when it has a connection path to the graph output node.", true);
    Prop_(Bool, Muted, "?Mute the node. This does not affect CPU load.", false);
    Prop(Float, Volume, 1.0);
    Prop_(Bool, Monitor, "?Plot the node's most recent input/output buffer(s).", false);

    struct SplitterDeleter {
        void operator()(ma_splitter_node *);
    };
    std::vector<std::unique_ptr<ma_splitter_node, SplitterDeleter>> SplitterNodes;

    struct MonitorDeleter {
        void operator()(ma_monitor_node *);
    };
    std::unique_ptr<ma_monitor_node, MonitorDeleter> OutputMonitorNode;
    std::unique_ptr<ma_monitor_node, MonitorDeleter> InputMonitorNode;

protected:
    void Render() const override;
    void RenderMonitor(IO) const;

    virtual ma_node *DoInit() { return nullptr; };
    virtual void DoUninit() {}

    void UpdateVolume();
    void UpdateMonitors();

    const AudioGraph *Graph;

    // `IsActive == true` means the audio device is on and there is a connection path from this node to the graph endpoint node (`OutputNode`).
    bool IsActive{false};

    // Cache the current sets of input and output nodes.
    std::unordered_set<const AudioGraphNode *> InputNodes;
    std::unordered_set<const AudioGraphNode *> OutputNodes;
};
