#pragma once

#include "AudioGraphAction.h"
#include "AudioGraphNode.h"
#include "Core/Container/AdjacencyList.h"
#include "Project/Audio/Faust/FaustDspChangeListener.h"

#include "Core/Container/Vector.h"
#include "Core/Primitive/UInt.h"

struct ma_node_graph;

struct MaGraph;
struct InputDeviceNode;
struct OutputDeviceNode;

struct AudioGraph : AudioGraphNode, Actionable<Action::AudioGraph::Any>, FaustDspChangeListener, AudioGraphNode::Listener {
    AudioGraph(ComponentArgs &&);
    ~AudioGraph();

    // Node overrides.
    // The graph is also a graph endpoint node.
    // The graph enforces that the only input to the graph endpoint node is the "Master" `OutputDeviceNode`.
    // The graph endpoint MA node is allocated and managed by the MA graph, unlike other node types whose MA counterparts are explicitly managed.
    bool AllowInputConnectionChange() const override { return false; }
    bool AllowOutputConnectionChange() const override { return false; }

    static std::unique_ptr<AudioGraphNode> CreateNode(Component *, string_view path_prefix_segment, string_view path_segment);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    void OnFieldChanged() override;
    void OnFaustDspChanged(dsp *) override;

    void OnNodeConnectionsChanged(AudioGraphNode *) override;

    ma_node_graph *Get() const;
    dsp *GetFaustDsp() const;

    bool IsNativeSampleRate(u32) const;
    u32 GetDefaultSampleRate() const;
    std::string GetSampleRateName(u32) const;

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

    std::unique_ptr<MaGraph> Graph;
    dsp *FaustDsp = nullptr;

    struct Connections : AdjacencyList {
        using AdjacencyList::AdjacencyList;

        void Render() const override;
    };

    Prop(Vector<AudioGraphNode>, Nodes, CreateNode);
    Prop_(Connections, Connections, "Audio connections");

    // We initialize with a sample rate of 0, which is the default sample rate. See `GetDefaultSampleRate` for details.
    Prop_(
        UInt, SampleRate,
        "?The audio graph sample rate.\n"
        "This is the rate at which the graph and all of the its nodes internally process audio.\n"
        "An asterisk (*) indicates the sample rate is natively supported by all audio device nodes within the graph.\n"
        "Each audio device I/O node within the graph converts to/from this rate if necessary.",
        [this](u32 sr) { return GetSampleRateName(sr); }
    );
    Prop(Style, Style);

    mutable ID SelectedNodeId{0}; // `Used for programatically navigating to nodes in the graph view.

private:
    // Returns the chosen node type-id, or `std::nullopt` if no node type was chosen.
    std::optional<std::string> RenderNodeCreateSelector() const;

    void Render() const override;

    void UpdateConnections();

    AudioGraphNode *FindByPathSegment(string_view) const;
    InputDeviceNode *GetInputDeviceNode() const;
    OutputDeviceNode *GetOutputDeviceNode() const;
};
