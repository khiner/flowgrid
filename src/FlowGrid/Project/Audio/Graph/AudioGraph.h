#pragma once

#include "AudioGraphNode.h"
#include "Core/Container/AdjacencyList.h"
#include "Project/Audio/Faust/FaustDspChangeListener.h"

#include "Project/Audio/AudioInputDevice.h"
#include "Project/Audio/AudioOutputDevice.h"

struct ma_device;
struct ma_node_graph;

struct MaGraph;
struct InputNode;
struct OutputNode;

struct AudioGraph : Component, Field::ChangeListener, FaustDspChangeListener, AudioGraphNode::Listener {
    AudioGraph(ComponentArgs &&);
    ~AudioGraph();

    void OnFieldChanged() override;
    void OnFaustDspChanged(dsp *) override;
    void OnNodeConnectionsChanged(AudioGraphNode *) override;

    static void AudioInputCallback(ma_device *, void *output, const void *input, u32 frame_count);
    static void AudioOutputCallback(ma_device *, void *output, const void *input, u32 frame_count);

    u32 GetDeviceSampleRate() const;
    u32 GetDeviceBufferSize() const;

    void RenderConnections() const;
    void RenderNodes() const;

    InputNode *GetInput() const;
    OutputNode *GetOutput() const;

    ma_node_graph *Get() const;

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

    Prop(AudioInputDevice, InputDevice, AudioInputCallback);
    Prop(AudioOutputDevice, OutputDevice, AudioOutputCallback);

    std::unique_ptr<MaGraph> Graph;
    std::vector<std::unique_ptr<AudioGraphNode>> Nodes;

    Prop(AdjacencyList, Connections);
    Prop(Style, Style);

private:
    void UpdateConnections();

    void Render() const override {}
};
