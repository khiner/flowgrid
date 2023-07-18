#include "AudioGraphNodes.h"

#include "AudioGraph.h"
#include "Project/Audio/AudioInputDevice.h"
#include "Project/Audio/Faust/FaustNode.h"
#include "Project/Audio/TestToneNode.h"

#include "miniaudio.h"

AudioGraphNodes::AudioGraphNodes(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {
    Nodes.push_back(std::make_unique<OutputNode>(ComponentArgs{this, "Output"}));
    Nodes.push_back(std::make_unique<InputNode>(ComponentArgs{this, "Input"}));
    Nodes.push_back(std::make_unique<FaustNode>(ComponentArgs{this, "Faust"}));
    Nodes.push_back(std::make_unique<TestToneNode>(ComponentArgs{this, "TestTone"}));
    Init();
}
AudioGraphNodes::~AudioGraphNodes() {
    Uninit();
}

// xxx depending on dynamic node positions is temporary.
void AudioGraphNodes::OnFaustDspChanged(dsp *dsp) { static_cast<FaustNode *>(Nodes[2].get())->OnFaustDspChanged(dsp); }
InputNode *AudioGraphNodes::GetInput() const { return static_cast<InputNode *>(Nodes[1].get()); }
OutputNode *AudioGraphNodes::GetOutput() const { return static_cast<OutputNode *>(Nodes[0].get()); }

void AudioGraphNodes::Init() {
    for (auto *node : *this) node->Init();
}
void AudioGraphNodes::Uninit() {
    for (auto *node : *this) node->Uninit();
}

void AudioGraphNodes::OnDeviceSampleRateChanged() {
    for (auto *node : *this) node->OnDeviceSampleRateChanged();
}

InputNode::InputNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Muted.Set_(true); // External input is muted by default.
}

struct InputNode::Buffer {
    Buffer(ma_format format, u32 channels) {
        int result = ma_audio_buffer_ref_init(format, channels, nullptr, 0, &BufferRef);
        if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize buffer ref: ", result));
    }
    ~Buffer() {
        ma_audio_buffer_ref_uninit(&BufferRef);
    }

    void SetData(const void *input, u32 frame_count) {
        ma_audio_buffer_ref_set_data(&BufferRef, input, frame_count);
    }

    ma_audio_buffer_ref *Get() noexcept { return &BufferRef; }

private:
    ma_audio_buffer_ref BufferRef;
};

void InputNode::SetBufferData(const void *input, u32 frame_count) const {
    if (_Buffer) _Buffer->SetData(input, frame_count);
}

ma_node *InputNode::DoInit(ma_node_graph *graph) {
    const AudioInputDevice &device = Graph->InputDevice;
    _Buffer = std::make_unique<Buffer>(ma_format(int(device.Format)), device.Channels);

    static ma_data_source_node source_node{}; // todo instance var
    ma_data_source_node_config config = ma_data_source_node_config_init(_Buffer->Get());
    int result = ma_data_source_node_init(graph, &config, nullptr, &source_node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

    return &source_node;
}

void InputNode::DoUninit() {
    _Buffer.reset();
    ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
}

// The output node is the graph endpoint. It's allocated and managed by the MA graph.
ma_node *OutputNode::DoInit(ma_node_graph *graph) {
    return ma_node_graph_get_endpoint(graph);
}

void AudioGraphNodes::Render() const {
    RenderTreeNodes();
}
