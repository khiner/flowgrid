#include "AudioGraphNodes.h"

#include "Project/Audio/AudioDevice.h"
#include "AudioGraph.h"

AudioGraphNodes::AudioGraphNodes(ComponentArgs &&args)
    : Component(std::move(args)), Graph(static_cast<const AudioGraph *>(Parent)) {}

AudioGraphNodes::~AudioGraphNodes() {}

void AudioGraphNodes::Init() {
    for (auto *node : *this) node->Init();
}
void AudioGraphNodes::Update() {
    for (auto *node : *this) node->Update();
}
void AudioGraphNodes::Uninit() {
    for (auto *node : *this) node->Uninit();
}

InputNode::InputNode(ComponentArgs &&args) : AudioGraphNode(std::move(args)) {
    Muted.Set_(true); // External input is muted by default.
}

ma_node *InputNode::DoInit() {
    Buffer = std::unique_ptr<ma_audio_buffer_ref, BufferDeleter>(new ma_audio_buffer_ref());
    int result = ma_audio_buffer_ref_init((ma_format) int(audio_device.InFormat), audio_device.InChannels, nullptr, 0, Buffer.get());
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize input audio buffer: ", result));

    static ma_data_source_node source_node{}; // todo instance var
    ma_data_source_node_config config = ma_data_source_node_config_init(Buffer.get());
    result = ma_data_source_node_init(Graph->Get(), &config, nullptr, &source_node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

    return &source_node;
}

void InputNode::BufferDeleter::operator()(ma_audio_buffer_ref *buffer) {
    ma_audio_buffer_ref_uninit(buffer);
}

void InputNode::DoUninit() {
    Buffer.reset();
    ma_data_source_node_uninit((ma_data_source_node *)Node, nullptr);
}

// The output node is the graph endpoint. It's allocated and managed by the MA graph.
ma_node *OutputNode::DoInit() {
    return ma_node_graph_get_endpoint(Graph->Get());
}

void AudioGraphNodes::Render() const {
    RenderTreeNodes();
}
