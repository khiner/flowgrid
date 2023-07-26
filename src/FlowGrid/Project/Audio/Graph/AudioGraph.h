#pragma once

#include "AudioGraphAction.h"
#include "AudioGraphNode.h"
#include "Core/Container/AdjacencyList.h"
#include "Project/Audio/Faust/FaustDspChangeListener.h"

#include "Core/Container/Vector.h"

struct ma_node_graph;

struct MaGraph;
struct DeviceInputNode;
struct DeviceOutputNode;

struct AudioGraph : AudioGraphNode, Actionable<Action::AudioGraph::Any>, FaustDspChangeListener, AudioGraphNode::Listener {
    AudioGraph(ComponentArgs &&);
    ~AudioGraph();

    // Node overrides.
    // The graph is also a graph endpoint node.
    // Technically, the graph endpoint node has an output bus, but it's handled specially by miniaudio.
    // Most importantly, it is not possible to attach the graph endpoint's node into any other node.
    // Thus, we treat it strictly as a sink and hide the fact that it technically has an output bus, since it functionally does not.
    // The graph enforces that the only input to the graph endpoint node is the "Master" `DeviceOutputNode`.
    // The graph endpoint MA node is allocated and managed by the MA graph.
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
    u32 GetSampleRate() const;
    u64 GetBufferSize() const;

    std::unique_ptr<MaGraph> Graph;
    dsp *FaustDsp = nullptr;

    Prop(Vector<AudioGraphNode>, Nodes, CreateNode);
    Prop(AdjacencyList, Connections);

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
                8, 4, 16
            );

        protected:
            void Render() const override;
        };

        Prop(Matrix, Matrix);
    };

    Prop(Style, Style);

private:
    void Render() const override;
    void RenderConnections() const;

    void UpdateConnections();

    AudioGraphNode *FindByPathSegment(string_view) const;
    DeviceInputNode *GetDeviceInputNode() const;
    DeviceOutputNode *GetDeviceOutputNode() const;
};
