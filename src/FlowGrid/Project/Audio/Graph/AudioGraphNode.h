#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Float.h"

#include "Project/Audio/AudioIO.h"

// xxx miniaudio should not be in a header.
#include "ma_gainer_node/ma_gainer_node.h"
#include "ma_monitor_node/ma_monitor_node.h"

struct AudioGraph;
using ma_node = void;

// using ma_node = void;
// struct ma_splitter_node;

enum WindowType_ {
    WindowType_Rectangular,
    // Cosine windows.
    WindowType_Hann,
    WindowType_Hamming,
    WindowType_Blackman,
    WindowType_BlackmanHarris,
    WindowType_Nuttall,
    WindowType_FlatTop,
    // Other windows, not parameterized.
    WindowType_Triangular,
    WindowType_Bartlett,
    WindowType_BartlettHann,
    WindowType_Bohman,
    WindowType_Parzen,
    // Other windows, parameterized.
    // We have implementations for these, but we're sticking with non-parameterized windows for now.
    // WindowType_Gaussian,
    // WindowType_Tukey,
    // WindowType_Taylor,
    // WindowType_Kaiser,
};

using WindowType = int;

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

    inline ma_node *InputNode() const noexcept {
        if (InputMonitorNode) return InputMonitorNode.get();
        return Node;
    }
    inline ma_node *OutputNode() const noexcept {
        if (OutputMonitorNode) return OutputMonitorNode.get();
        if (GainerNode) return GainerNode.get();
        return Node;
    }

    void ConnectTo(AudioGraphNode &);
    void DisconnectAll();

    inline void SetActive(bool is_active) noexcept { IsActive = is_active; }

    void Init();
    void Update();
    void Uninit();

    Prop_(Bool, On, "?When a node is off, it is completely removed from the audio graph.\nIt is active when it has a connection path to the graph output node.", true);
    Prop_(Bool, Muted, "?Mute the node. This does not affect CPU load.", false);
    Prop(Float, OutputLevel, 1.0);
    Prop(Bool, SmoothOutputLevel, true);
    Prop(Float, SmoothOutputLevelMs, 30);

    Prop_(Bool, Monitor, "?Plot the node's most recent input/output buffer(s).", false);
    Prop_(
        Enum, WindowType, "?The window type used for the magnitude spectrum FFT.",
        {"Rectangular", "Hann", "Hamming", "Blackman", "Blackman-Harris", "Nuttall", "Flat-Top", "Triangular", "Bartlett", "Bartlett-Hann", "Bohman", "Parzen"},
        WindowType_BlackmanHarris
    );

    struct GainerDeleter {
        void operator()(ma_gainer_node *);
    };
    std::unique_ptr<ma_gainer_node, GainerDeleter> GainerNode;

    struct SplitterDeleter {
        void operator()(ma_splitter_node *);
    };
    std::vector<std::unique_ptr<ma_splitter_node, SplitterDeleter>> SplitterNodes;

    struct MonitorDeleter {
        void operator()(ma_monitor_node *);
    };
    std::unique_ptr<ma_monitor_node, MonitorDeleter> OutputMonitorNode;
    std::unique_ptr<ma_monitor_node, MonitorDeleter> InputMonitorNode;

    // The remaining fields are derived from graph connections and are updated via `AudioGraph::UpdateConnections()`.

    // `IsActive == true` means the audio device is on and there is a connection path from this node to the graph endpoint node (`OutputNode`).
    bool IsActive{false};
    // Cache the current sets of input and output nodes.
    std::unordered_set<const AudioGraphNode *> InputNodes, OutputNodes;

protected:
    void Render() const override;
    void RenderMonitorWaveform(IO) const;
    void RenderMonitorMagnitudeSpectrum(IO) const;

    virtual ma_node *DoInit() { return nullptr; };
    virtual void DoUninit() {}

    void UpdateOutputLevel();
    void UpdateGainer();
    void UpdateMonitors();
    void UpdateMonitorSampleRate(IO);
    void UpdateMonitorWindowFunction(IO);

    const AudioGraph *Graph;
};
