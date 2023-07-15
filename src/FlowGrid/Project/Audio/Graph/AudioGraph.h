#pragma once

#include "AudioGraphNodes.h"
#include "Core/Container/AdjacencyList.h"
#include "Project/Audio/Faust/FaustDspChangeListener.h"

struct ma_device;
struct ma_node_graph;

struct AudioGraph : Component, Field::ChangeListener, FaustDspChangeListener, AudioGraphNode::Listener {
    AudioGraph(ComponentArgs &&);
    ~AudioGraph();

    void OnFieldChanged() override;
    void OnFaustDspChanged(dsp *) override;
    void OnNodeConnectionsChanged(AudioGraphNode *) override;

    void AudioCallback(ma_device *, void *output, const void *input, u32 frame_count) const;

    void RenderConnections() const;

    // Wraps around `ma_node_graph`.
    struct MaGraph {
        MaGraph();
        ~MaGraph();

        void Init();
        void Uninit();

        inline ma_node_graph *Get() const noexcept { return Graph.get(); }
        std::unique_ptr<ma_node_graph> Graph;
    };

    inline ma_node_graph *Get() const noexcept { return Graph.Get(); }

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

    MaGraph Graph;

    Prop(AudioGraphNodes, Nodes);
    Prop(AdjacencyList, Connections);
    Prop(Style, Style);

private:
    void UpdateConnections();

    void Render() const override {}
};
