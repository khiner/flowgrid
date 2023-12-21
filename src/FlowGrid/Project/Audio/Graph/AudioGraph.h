#pragma once

#include "AudioGraphAction.h"
#include "AudioGraphNode.h"
#include "Core/Action/ActionProducer.h"
#include "Core/Container/AdjacencyList.h"
#include "Core/ProducerComponentArgs.h"
#include "Project/Audio/Device/DeviceDataFormat.h"
#include "Project/Audio/Faust/FaustDSPListener.h"

#include "Core/Container/Vector.h"

struct ma_node_graph;

struct InputDeviceNode;
struct OutputDeviceNode;

inline static const std::string InputDeviceNodeTypeId = "Input";
inline static const std::string OutputDeviceNodeTypeId = "Output";
inline static const std::string WaveformNodeTypeId = "Waveform";
inline static const std::string FaustNodeTypeId = "Faust";

struct AudioGraph
    : AudioGraphNode,
      Actionable<Action::AudioGraph::Any>,
      ActionProducer<Action::AudioGraph::Any>,
      FaustDSPListener,
      AudioGraphNode::Listener {
    AudioGraph(ProducerComponentArgs<ProducedActionType> &&);
    ~AudioGraph();

    // Node overrides.
    // The graph is also a graph endpoint node.
    // The graph enforces that the only input to the graph endpoint node is the "Master" `OutputDeviceNode`.
    // The graph endpoint MA node is allocated and managed by the MA graph, unlike other node types whose MA counterparts are explicitly managed.
    bool AllowInputConnectionChange() const override { return false; }
    bool AllowOutputConnectionChange() const override { return false; }

    static std::unique_ptr<AudioGraphNode> CreateAudioGraphNode(ComponentArgs &&);

    std::unique_ptr<MaNode> CreateNode() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    void OnComponentChanged() override;

    void OnFaustDspChanged(ID, dsp *) override;
    void OnFaustDspAdded(ID, dsp *) override;
    void OnFaustDspRemoved(ID) override;

    void OnNodeConnectionsChanged(AudioGraphNode *) override;

    ma_node_graph *Get();
    dsp *GetFaustDsp(ID id) const;

    // A sample rate is considered "native" by the graph (and suffixed with an asterix)
    // if it is native to all device nodes within the graph (or if there are no device nodes in the graph).
    bool IsNativeSampleRate(u32) const;

    // Returns the highest-priority sample rate (see `AudioDevice::PrioritizedSampleRates`) natively supported by all device nodes in this graph,
    // or the highest-priority sample rate supported by any device node if none are natively supported by all device nodes.
    u32 GetDefaultSampleRate() const;
    std::string GetSampleRateName(u32) const;
    DeviceDataFormat GetDeviceClientFormat(IO) const;

    u32 GetBufferFrames() const;

    std::unordered_set<AudioGraphNode *> GetSourceNodes(const AudioGraphNode *) const;
    std::unordered_set<AudioGraphNode *> GetDestinationNodes(const AudioGraphNode *) const;

    struct Style : Component {
        using Component::Component;

        struct Matrix : Component {
            using Component::Component;

            Prop_(Float, CellSize, "?The size of each matrix cell, as a multiple of line height.", 1, 1, 3);
            Prop_(Float, CellGap, "?The gap between matrix cells.", 1, 0, 10);
            Prop_(
                Float, MaxLabelSpace,
                "?The matrix is placed to make room for the longest input/output node label, up to this limit (as a multiple of line height), at which point the labels will be ellipsified.\n"
                "(Use Style->ImGui->InnerItemSpacing->X for spacing between labels and cells.)",
                8, 3, 16
            );

        protected:
            void Render() const override;
        };

        Prop(Matrix, Matrix);
    };

    dsp *FaustDsp = nullptr;

    struct Connections : AdjacencyList {
        using AdjacencyList::AdjacencyList;

        void Render() const override;
    };

    struct ChannelConverterNode {
        ChannelConverterNode(AudioGraph *, u32 from_channels, u32 to_channels);
        ~ChannelConverterNode();

        ma_channel_converter_node *Get() const;

        u32 ChannelCount(IO) const;

    private:
        AudioGraph *Graph;
        std::unique_ptr<ma_channel_converter_node> Converter;
    };

    Prop(Vector<AudioGraphNode>, Nodes, CreateAudioGraphNode);
    Prop_(Connections, Connections, "Audio connections");

    // We initialize with a sample rate of zero, which is the default sample rate. (See `GetDefaultSampleRate` for details.)
    // All device nodes with a default format (for which the user has not explicitly selected the format
    // from the device's format dropdown) "follow" their owning graph's sample rate.
    // When a graph's sample rate is changed, each of its device nodes is updated to select the native format with the
    // sample rate nearest to the graph's.
    // Device nodes which have had their format explicitly chosen by the user are considered "locked in", and are not
    // automatically updated when its owning graph's sample rate changes - even if it has a native format with a matching sample rate.
    Prop_(
        UInt, SampleRate,
        "?The rate at which the graph and all of the its nodes internally process audio.\n"
        "An asterisk (*) indicates the sample rate is natively supported by all audio device nodes within the graph.\n"
        "Each audio device I/O node within the graph converts to/from this rate if necessary.",
        [this](u32 sr) { return GetSampleRateName(sr); },
        176400
    );
    Prop(Style, Style);

    mutable ID SelectedNodeId{0}; // `Used for programatically navigating to nodes in the graph view.

private:
    void Render() const override;
    void RenderNodeCreateSelector() const;

    void UpdateConnections();
    void Connect(ma_node *source, u32 source_output_bus, ma_node *destination, u32 destination_input_bus);

    AudioGraphNode *FindByPathSegment(string_view path_segment) const {
        auto node_it = std::find_if(Nodes.begin(), Nodes.end(), [path_segment](const auto *node) { return node->PathSegment == path_segment; });
        return node_it != Nodes.end() ? node_it->get() : nullptr;
    }
    auto FindAllByPathSegment(string_view path_segment) const {
        return Nodes.View() | std::views::filter([path_segment](const auto &node) { return node->PathSegment == path_segment; });
    }

    // We don't support creating multiple input/output nodes yet, so in reality there will be at most one of each for now.
    auto GetInputDeviceNodes() const {
        return FindAllByPathSegment(InputDeviceNodeTypeId) |
            std::views::transform([](const auto &node) { return reinterpret_cast<InputDeviceNode *>(node.get()); });
    }
    auto GetOutputDeviceNodes() const {
        return FindAllByPathSegment(OutputDeviceNodeTypeId) |
            std::views::transform([](const auto &node) { return reinterpret_cast<OutputDeviceNode *>(node.get()); });
    }

    std::vector<std::unique_ptr<ChannelConverterNode>> ChannelConverterNodes;
    std::unordered_map<ID, dsp *> DspById;
};
