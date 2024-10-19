#pragma once

#include "miniaudio.h"

/* Based on `ma_data_source_node`. 1 input bus, 1 output bus. Captures most recently read input buffer in a buffer ref. */
struct ma_data_passthrough_node_config {
    ma_node_config node_config;
    ma_uint32 channels;
    ma_audio_buffer_ref *buffer_ref;
};

// If `buffer_ref` is empty, this will be a passthrough node. Otherwise, the output will be silenced.
ma_data_passthrough_node_config ma_data_passthrough_node_config_init(ma_uint32 channels, ma_audio_buffer_ref *buffer_ref);

struct ma_data_passthrough_node {
    ma_node_base base;
    ma_audio_buffer_ref *buffer_ref;
};

ma_result ma_data_passthrough_node_init(ma_node_graph *, const ma_data_passthrough_node_config *, const ma_allocation_callbacks *, ma_data_passthrough_node *);
void ma_data_passthrough_node_uninit(ma_data_passthrough_node *, const ma_allocation_callbacks *);
