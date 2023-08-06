#include "ma_data_passthrough_node.h"

#include "../ma_helper.h"

ma_data_passthrough_node_config ma_data_passthrough_node_config_init(ma_uint32 channels, ma_audio_buffer_ref *buffer_ref) {
    ma_data_passthrough_node_config config;

    MA_ZERO_OBJECT(&config);
    config.node_config = ma_node_config_init();
    config.channels = channels;
    config.buffer_ref = buffer_ref;

    return config;
}

static void ma_data_passthrough_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    auto *passthrough = (ma_data_passthrough_node *)node;
    if (passthrough->buffer_ref) ma_audio_buffer_ref_set_data(passthrough->buffer_ref, frames_in[0], *frame_count_out);

    (void)frames_out;
    (void)frame_count_in;
}

ma_result ma_data_passthrough_node_init(ma_node_graph *pNodeGraph, const ma_data_passthrough_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_data_passthrough_node *passthrough) {
    if (passthrough == nullptr || config == nullptr) return MA_INVALID_ARGS;
    if (config->buffer_ref && config->buffer_ref->channels != config->channels) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(passthrough);

    static ma_node_vtable passthrough_vtable = {ma_data_passthrough_node_process_pcm_frames, nullptr, 1, 1, MA_NODE_FLAG_PASSTHROUGH};
    static ma_node_vtable silenced_vtable = {ma_data_passthrough_node_process_pcm_frames, nullptr, 1, 1, MA_NODE_FLAG_SILENT_OUTPUT};

    ma_uint32 channels = config->channels;
    ma_node_config base_config = config->node_config;
    base_config.vtable = config->buffer_ref ? &silenced_vtable : &passthrough_vtable;
    base_config.pInputChannels = &channels;
    base_config.pOutputChannels = &channels;

    ma_result result = ma_node_init(pNodeGraph, &base_config, allocation_callbacks, &passthrough->base);
    if (result != MA_SUCCESS) return result;

    passthrough->buffer_ref = config->buffer_ref;

    return MA_SUCCESS;
}

void ma_data_passthrough_node_uninit(ma_data_passthrough_node *passthrough, const ma_allocation_callbacks *allocation_callbacks) {
    ma_node_uninit(&passthrough->base, allocation_callbacks);
}
