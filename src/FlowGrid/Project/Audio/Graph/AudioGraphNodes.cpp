#include "AudioGraphNodes.h"

#include "AudioGraph.h"
#include "Project/Audio/AudioDevice.h"

#include "miniaudio.h"

AudioGraphNodes::AudioGraphNodes(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {
    Init();
}
AudioGraphNodes::~AudioGraphNodes() {
    Uninit();
}

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

ma_node *InputNode::DoInit() {
    const AudioDevice &device = Graph->Device;
    _Buffer = std::make_unique<Buffer>(ma_format(int(device.InFormat)), device.InChannels);

    static ma_data_source_node source_node{}; // todo instance var
    ma_data_source_node_config config = ma_data_source_node_config_init(_Buffer->Get());
    int result = ma_data_source_node_init(Graph->Get(), &config, nullptr, &source_node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

    return &source_node;
}

void InputNode::DoUninit() {
    _Buffer.reset();
    ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
}

// The output node is the graph endpoint. It's allocated and managed by the MA graph.
ma_node *OutputNode::DoInit() {
    return ma_node_graph_get_endpoint(Graph->Get());
}

void AudioGraphNodes::Render() const {
    RenderTreeNodes();
}
