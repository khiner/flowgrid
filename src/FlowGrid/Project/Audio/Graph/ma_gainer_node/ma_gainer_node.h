#pragma once

#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
    ma_node_config node_config;
    ma_gainer_config gainer_config;
} ma_gainer_node_config;

ma_gainer_node_config ma_gainer_node_config_init(ma_uint32 channels, ma_uint32 smooth_time_frames);

typedef struct
{
    ma_node_base base_node;
    ma_gainer_node_config config;
    ma_gainer gainer;
} ma_gainer_node;

ma_result ma_gainer_node_init(ma_node_graph *, const ma_gainer_node_config *, const ma_allocation_callbacks *, ma_gainer_node *);
void ma_gainer_node_uninit(ma_gainer_node *, const ma_allocation_callbacks *);

ma_result ma_gainer_node_set_gain(ma_gainer_node *, float volume);

#ifdef __cplusplus
}
#endif
