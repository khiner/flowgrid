#pragma once

#include "AudioGraphNode.h"

// todo dynamic node creation with no dependencies on any specific node types.
#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/TestToneNode.h"

struct InputNode : AudioGraphNode {
    InputNode(ComponentArgs &&);

    ma_node *DoInit() override;
    void DoUninit() override;

    void SetBufferData(const void *input, u32 frame_count) const;

    struct Buffer;

private:
    std::unique_ptr<Buffer> _Buffer;
};

struct OutputNode : AudioGraphNode {
    using AudioGraphNode::AudioGraphNode;

    ma_node *DoInit() override;
};

struct AudioGraphNodes : Component {
    AudioGraphNodes(ComponentArgs &&);
    ~AudioGraphNodes();

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

    const AudioGraph *Graph;

    // Order declarations from early to late in the signal path.
    // This has the benefit of making it faster to disconnect the output buses of all nodes.
    // From the "Thread Safety and Locking" section of the miniaudio docs:
    // "The cost of detaching nodes earlier in the pipeline (data sources, for example) will be cheaper than the cost of detaching higher
    //  level nodes, such as some kind of final post-processing endpoint.
    //  If you need to do mass detachments, detach starting from the lowest level nodes and work your way towards the final endpoint node."
    Prop(TestToneNode, TestTone);
    Prop(InputNode, Input); // `ma_data_source_node` whose `ma_data_source` is a `ma_audio_buffer_ref` pointing directly to the input buffer.
    Prop(FaustNode, Faust);
    Prop(OutputNode, Output);

private:
    void Render() const override;
};
