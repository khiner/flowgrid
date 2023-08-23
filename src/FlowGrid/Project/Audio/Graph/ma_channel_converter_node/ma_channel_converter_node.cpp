#include "ma_channel_converter_node.h"

#include "../ma_helper.h"

ma_channel_converter_node_config ma_channel_converter_node_config_init(ma_uint32 in_channels, ma_uint32 out_channels) {
    ma_channel_converter_node_config config;
    config.node_config = ma_node_config_init();
    config.converter_config = ma_channel_converter_config_init(ma_format_f32, in_channels, nullptr, out_channels, nullptr, ma_channel_mix_mode_default);

    return config;
}

static void ma_channel_converter_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    auto *converter_node = (ma_channel_converter_node *)node;
    ma_channel_converter_process_pcm_frames(&converter_node->converter, frames_out[0], frames_in[0], *frame_count_out);
    (void)frame_count_in;
}

ma_result ma_channel_converter_node_init(ma_node_graph *graph, const ma_channel_converter_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_channel_converter_node *converter_node) {
    if (converter_node == nullptr || config == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(converter_node);
    converter_node->config = *config;

    ma_result result = ma_channel_converter_init(&config->converter_config, allocation_callbacks, &converter_node->converter);
    if (result != MA_SUCCESS) return result;

    static const ma_node_vtable vtable = {ma_channel_converter_node_process_pcm_frames, nullptr, 1, 1, 0};
    ma_node_config base_config;

    base_config = config->node_config;
    base_config.vtable = &vtable;
    ma_uint32 input_channels[1] = {config->converter_config.channelsIn};
    ma_uint32 output_channels[1] = {config->converter_config.channelsOut};
    base_config.pInputChannels = input_channels;
    base_config.pOutputChannels = output_channels;

    result = ma_node_init(graph, &base_config, allocation_callbacks, converter_node);
    if (result != MA_SUCCESS) return result;

    return MA_SUCCESS;
}

void ma_channel_converter_node_uninit(ma_channel_converter_node *converter_node, const ma_allocation_callbacks *allocation_callbacks) {
    if (converter_node == nullptr) return;

    ma_channel_converter_uninit(&converter_node->converter, allocation_callbacks);
    ma_node_uninit(converter_node, allocation_callbacks);
}
