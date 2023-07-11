#pragma once

#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 bufferSizeInFrames;
} ma_monitor_node_config;

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 bufferSizeInFrames);

typedef struct
{
    ma_node_base baseNode;
    ma_uint32 bufferSizeInFrames;
    float *pBuffer;
} ma_monitor_node;

ma_result ma_monitor_node_init(ma_node_graph *, const ma_monitor_node_config *, const ma_allocation_callbacks *, ma_monitor_node *);
void ma_monitor_node_uninit(ma_monitor_node *, const ma_allocation_callbacks *);
#ifdef __cplusplus
}
#endif
