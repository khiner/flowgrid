#include "ma_gainer_node.h"

#include "../ma_helper.h"

ma_gainer_node_config ma_gainer_node_config_init(ma_uint32 channels, float gain, ma_uint32 smooth_time_frames) {
    ma_gainer_node_config config;
    config.node_config = ma_node_config_init();
    config.gainer_config = ma_gainer_config_init(channels, smooth_time_frames);
    config.gain = gain;

    return config;
}

ma_result ma_gainer_node_set_gain(ma_gainer_node *gainer_node, float gain) {
    if (gainer_node == nullptr) return MA_INVALID_ARGS;

    return ma_gainer_set_gain(&gainer_node->gainer, gain);
}

static void ma_gainer_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    ma_gainer_node *gainer_node = (ma_gainer_node *)node;
    ma_gainer_process_pcm_frames(&gainer_node->gainer, frames_out[0], frames_in[0], *frame_count_out);

    (void)frame_count_in;
}

ma_result ma_gainer_node_init(ma_node_graph *node_graph, const ma_gainer_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_gainer_node *gainer_node) {
    if (gainer_node == nullptr || config == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(gainer_node);
    gainer_node->config = *config;

    ma_result result = ma_gainer_init(&config->gainer_config, allocation_callbacks, &gainer_node->gainer);
    if (result != MA_SUCCESS) return result;

    static ma_node_vtable vtable = {ma_gainer_node_process_pcm_frames, nullptr, 1, 1, 0};
    ma_node_config base_config = config->node_config;
    base_config.vtable = &vtable;
    base_config.pInputChannels = &config->gainer_config.channels;
    base_config.pOutputChannels = &config->gainer_config.channels;

    result = ma_node_init(node_graph, &base_config, allocation_callbacks, gainer_node);
    ma_gainer_set_gain(&gainer_node->gainer, gainer_node->config.gain);
    return result;
}

void ma_gainer_node_uninit(ma_gainer_node *gainer_node, const ma_allocation_callbacks *allocation_callbacks) {
    if (gainer_node == nullptr) return;

    ma_gainer_uninit(&gainer_node->gainer, allocation_callbacks);
    ma_node_uninit(gainer_node, allocation_callbacks);
}
