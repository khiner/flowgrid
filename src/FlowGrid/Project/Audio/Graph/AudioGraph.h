#pragma once

#include "AudioGraphNodes.h"
#include "Core/Container/AdjacencyList.h"
#include "Project/Audio/Faust/FaustDspChangeListener.h"

struct ma_device;
struct ma_node_graph;

// Corresponds to `ma_node_graph`.
struct AudioGraph : Component, Field::ChangeListener, FaustDspChangeListener {
    AudioGraph(ComponentArgs &&);
    ~AudioGraph();

    void OnFieldChanged() override;
    void OnFaustDspChanged(dsp *) override;

    void AudioCallback(ma_device *, void *output, const void *input, Count frame_count) const;

    void RenderConnections() const;
    void Update();

    inline ma_node_graph *Get() const noexcept { return Graph.get(); }

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

    Prop(AudioGraphNodes, Nodes);
    Prop(AdjacencyList, Connections);
    Prop(Style, Style);

    std::unique_ptr<ma_node_graph> Graph;

private:
    void Init();
    void Uninit();
    void UpdateConnections();

    void Render() const override {}
};
