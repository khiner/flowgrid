#include "ma_panner_node.h"

#include "../ma_helper.h"

ma_panner_node_config ma_panner_node_config_init(ma_uint32 in_channels, ma_pan_mode mode) {
    ma_panner_node_config config;
    config.node_config = ma_node_config_init();
    config.panner_config = ma_panner_config_init(ma_format_f32, 2);
    config.panner_config.mode = mode;
    config.in_channels = in_channels;
    return config;
}

ma_result ma_panner_node_set_pan(ma_panner_node *panner_node, float pan) {
    if (panner_node == nullptr) return MA_INVALID_ARGS;

    ma_panner_set_pan(&panner_node->panner, pan);
    return MA_SUCCESS;
}

ma_result ma_panner_node_set_mode(ma_panner_node *panner_node, ma_pan_mode mode) {
    if (panner_node == nullptr) return MA_INVALID_ARGS;

    ma_panner_set_mode(&panner_node->panner, mode);
    return MA_SUCCESS;
}

static void ma_panner_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    auto *panner_node = (ma_panner_node *)node;
    if (panner_node->converter) {
        ma_channel_converter_process_pcm_frames(panner_node->converter.get(), frames_out[0], frames_in[0], *frame_count_out);
        ma_panner_process_pcm_frames(&panner_node->panner, frames_out[0], frames_out[0], *frame_count_out);
    } else {
        ma_panner_process_pcm_frames(&panner_node->panner, frames_out[0], frames_in[0], *frame_count_out);
    }
    (void)frame_count_in;
}

ma_result ma_panner_node_init(ma_node_graph *graph, const ma_panner_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_panner_node *panner_node) {
    if (panner_node == nullptr || config == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(panner_node);
    panner_node->config = *config;

    ma_result result = ma_panner_init(&config->panner_config, &panner_node->panner);
    if (result != MA_SUCCESS) return result;

    if (config->in_channels != 2) {
        panner_node->converter = std::make_unique<ma_channel_converter>();
        auto converter_config = ma_channel_converter_config_init(ma_format_f32, config->in_channels, nullptr, 2, nullptr, ma_channel_mix_mode_default);
        result = ma_channel_converter_init(&converter_config, allocation_callbacks, panner_node->converter.get());
        if (result != MA_SUCCESS) return result; // There is no `ma_panner_uninit`.
    }

    static const ma_node_vtable vtable = {ma_panner_node_process_pcm_frames, nullptr, 1, 1, 0};
    ma_node_config base_config;

    base_config = config->node_config;
    base_config.vtable = &vtable;
    ma_uint32 input_channels[1] = {config->in_channels};
    ma_uint32 output_channels[1] = {2};
    base_config.pInputChannels = input_channels;
    base_config.pOutputChannels = output_channels;

    result = ma_node_init(graph, &base_config, allocation_callbacks, panner_node);
    if (result != MA_SUCCESS) {
        if (panner_node->converter) ma_channel_converter_uninit(panner_node->converter.get(), allocation_callbacks);
        return result;
    }

    return MA_SUCCESS;
}

void ma_panner_node_uninit(ma_panner_node *panner_node, const ma_allocation_callbacks *allocation_callbacks) {
    if (panner_node == nullptr) return;

    // There is no `ma_panner_uninit`.
    if (panner_node->converter) ma_channel_converter_uninit(panner_node->converter.get(), allocation_callbacks);
    ma_node_uninit(panner_node, allocation_callbacks);
}
