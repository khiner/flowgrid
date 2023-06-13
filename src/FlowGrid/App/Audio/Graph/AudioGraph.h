#pragma once

#include "App/Audio/Faust/FaustNode.h" // xxx should not depend on any specific node types
#include "AudioGraphNode.h"
#include "Core/Field/Float.h"
#include "Core/Field/Matrix.h"

struct ma_device;

// Corresponds to `ma_node_graph`.
struct AudioGraph : Component, Drawable {
    using Component::Component;

    void Init() const;
    void Update() const;
    void Uninit() const;

    static void AudioCallback(ma_device *, void *output, const void *input, Count frame_count);

    struct InputNode : AudioGraphNode {
        using AudioGraphNode::AudioGraphNode;
        void DoInit(ma_node_graph *) const override;
        void DoUninit() const override;
    };

    struct Nodes : Component, Drawable {
        using Component::Component;

        // Iterate over all children, converting each element from a `Component *` to a `Node *`.
        // Usage: `for (const Node *node : Nodes) ...`
        struct Iterator : std::vector<Component *>::const_iterator {
            Iterator(auto it) : std::vector<Component *>::const_iterator(it) {}
            const AudioGraphNode *operator*() const { return dynamic_cast<const AudioGraphNode *>(std::vector<Component *>::const_iterator::operator*()); }
        };
        Iterator begin() const { return Children.cbegin(); }
        Iterator end() const { return Children.cend(); }

        void Init() const;
        void Update() const;
        void Uninit() const;

        // `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref` pointing directly to the input buffer.
        // todo configurable data source
        Prop(InputNode, Input);
        Prop(FaustNode, Faust);
        Prop(AudioGraphNode, Output);

    protected:
        void Render() const override;
    };

    struct Style : Component {
        using Component::Component;

        struct Matrix : Component, Drawable {
            using Component::Component;

            Prop_(Float, CellSize, "?The size of each matrix cell, as a multiple of line height.", 1, 1, 3);
            Prop_(Float, CellGap, "?The gap between matrix cells.", 1, 0, 10);
            Prop_(Float, LabelSize, "?The space provided for the label, as a multiple of line height.\n(Use Style->ImGui->InnerItemSpacing->X for spacing between labels and cells.)", 6, 3, 8);

        protected:
            void Render() const override;
        };

        Prop(Matrix, Matrix);
    };

    Prop(Nodes, Nodes);
    Prop(Matrix<bool>, Connections);
    Prop(Style, Style);

protected:
    void Render() const override;
    void RenderConnections() const;
};
