#pragma once

#include "AudioGraphNode.h"

#include "Project/Audio/Faust/FaustDspChangeListener.h"

// `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref` pointing directly to the input buffer.
struct InputNode : AudioGraphNode {
    InputNode(ComponentArgs &&);

    ma_node *DoInit(ma_node_graph *) override;
    void DoUninit() override;

    void SetBufferData(const void *input, u32 frame_count) const;

    struct Buffer;

private:
    std::unique_ptr<Buffer> _Buffer;
};

struct OutputNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;

    ma_node *DoInit(ma_node_graph *) override;
};

struct AudioGraphNodes : Component, FaustDspChangeListener {
    AudioGraphNodes(ComponentArgs &&);
    ~AudioGraphNodes();

    void OnFaustDspChanged(dsp *) override;

    // Iterate over all children, converting each element from a `Component *` to a `Node *`.
    // Usage: `for (const Node *node : Nodes) ...`
    struct Iterator : std::vector<Component *>::const_iterator {
        Iterator(auto it) : std::vector<Component *>::const_iterator(it) {}
        const AudioGraphNode *operator*() const { return static_cast<const AudioGraphNode *>(std::vector<Component *>::const_iterator::operator*()); }
        AudioGraphNode *operator*() { return static_cast<AudioGraphNode *>(std::vector<Component *>::const_iterator::operator*()); }
    };
    Iterator begin() const { return Children.cbegin(); }
    Iterator end() const { return Children.cend(); }

    Iterator begin() { return Children.begin(); }
    Iterator end() { return Children.end(); }

    void Init();
    void Uninit();

    void OnDeviceSampleRateChanged();

    InputNode *GetInput() const;
    OutputNode *GetOutput() const;

    const AudioGraph *Graph;
    std::vector<std::unique_ptr<AudioGraphNode>> Nodes;

private:
    void Render() const override;
};
