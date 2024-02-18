#include "ma_waveform_node.h"

#include "../ma_helper.h"

ma_waveform_node_config ma_waveform_node_config_init(ma_uint32 sample_rate, ma_waveform_type type, double frequency) {
    ma_waveform_node_config config;
    config.node_config = ma_node_config_init();
    config.waveform_config = ma_waveform_config_init(ma_format_f32, 1, sample_rate, type, 1, frequency);

    return config;
}

ma_result ma_waveform_node_set_sample_rate(ma_waveform_node *waveform_node, ma_uint32 sample_rate) {
    if (waveform_node == nullptr) return MA_INVALID_ARGS;

    return ma_waveform_set_sample_rate(&waveform_node->waveform, sample_rate);
}

static void ma_waveform_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    ma_waveform_node *waveform_node = (ma_waveform_node *)node;
    ma_waveform_read_pcm_frames(&waveform_node->waveform, frames_out[0], frame_count_out[0], nullptr);

    (void)frame_count_in;
    (void)frames_in;
}

ma_result ma_waveform_node_init(ma_node_graph *node_graph, const ma_waveform_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_waveform_node *waveform_node) {
    if (waveform_node == nullptr || config == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(waveform_node);
    waveform_node->config = *config;
    if (ma_result result = ma_waveform_init(&config->waveform_config, &waveform_node->waveform); result != MA_SUCCESS) return result;

    static ma_node_vtable vtable = {ma_waveform_node_process_pcm_frames, nullptr, 0, 1, 0};
    ma_node_config base_config = config->node_config;
    base_config.vtable = &vtable;
    static ma_uint32 in_channels = 0;
    base_config.pInputChannels = &in_channels;
    base_config.pOutputChannels = &config->waveform_config.channels;

    return ma_node_init(node_graph, &base_config, allocation_callbacks, &waveform_node->base);
}

void ma_waveform_node_uninit(ma_waveform_node *waveform_node, const ma_allocation_callbacks *allocation_callbacks) {
    ma_node_uninit(&waveform_node->base, allocation_callbacks);
}
