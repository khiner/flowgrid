#pragma once

#include "miniaudio.h"

/* Based on `ma_data_source_node`. 1 input bus, 1 output bus. Holds its most recently read output buffer. */
struct ma_data_passthrough_node_config {
    ma_node_config nodeConfig;
    ma_audio_buffer_ref *pDataSource;
};

MA_API ma_data_passthrough_node_config ma_data_passthrough_node_config_init(ma_audio_buffer_ref *pDataSource);

struct ma_data_passthrough_node {
    ma_node_base base;
    ma_audio_buffer_ref *pDataSource;
};

ma_result ma_data_passthrough_node_init(ma_node_graph *pNodeGraph, const ma_data_passthrough_node_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_data_passthrough_node *pDataPassthroughNode);
void ma_data_passthrough_node_uninit(ma_data_passthrough_node *pDataPassthroughNode, const ma_allocation_callbacks *pAllocationCallbacks);
