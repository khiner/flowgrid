#include "ma_monitor_node.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static MA_INLINE void ma_zero_memory_default(void *p, size_t sz) {
    if (p == NULL) {
        assert(sz == 0);
        return;
    }

    if (sz > 0) {
        memset(p, 0, sz);
    }
}

#define MA_ZERO_OBJECT(p) ma_zero_memory_default((p), sizeof(*(p)))

#define MIN(a, b) ((a) < (b) ? (a) : (b))

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 bufferSizeInFrames) {
    ma_monitor_node_config config;
    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_node_config_init(); // Input and output channels are set in ma_monitor_node_init().
    config.channels = channels;
    config.sampleRate = sampleRate;
    config.bufferSizeInFrames = bufferSizeInFrames;

    return config;
}

static void ma_monitor_node_process_pcm_frames(ma_node *pNode, const float **ppFramesIn, ma_uint32 *pFrameCountIn, float **ppFramesOut, ma_uint32 *pFrameCountOut) {
    (void)pFrameCountIn;
    (void)ppFramesIn;
    ma_monitor_node *pMonitorNode = (ma_monitor_node *)pNode;
    ma_copy_pcm_frames(pMonitorNode->pBuffer, ppFramesOut[0], MIN(*pFrameCountOut, pMonitorNode->bufferSizeInFrames), ma_format_f32, 1);
}

static ma_node_vtable g_ma_monitor_node_vtable = {ma_monitor_node_process_pcm_frames, NULL, 1, 1, MA_NODE_FLAG_PASSTHROUGH};

ma_result ma_monitor_node_init(ma_node_graph *pNodeGraph, const ma_monitor_node_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_monitor_node *pMonitorNode) {
    if (pMonitorNode == NULL) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(pMonitorNode);

    if (pConfig == NULL) return MA_INVALID_ARGS;

    pMonitorNode->bufferSizeInFrames = pConfig->bufferSizeInFrames;
    pMonitorNode->pBuffer = (float *)ma_malloc((size_t)(pMonitorNode->bufferSizeInFrames * ma_get_bytes_per_frame(ma_format_f32, pConfig->channels)), pAllocationCallbacks);
    if (pMonitorNode->pBuffer == NULL) return MA_OUT_OF_MEMORY;

    ma_silence_pcm_frames(pMonitorNode->pBuffer, pMonitorNode->bufferSizeInFrames, ma_format_f32, pConfig->channels);

    ma_node_config baseConfig;
    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable = &g_ma_monitor_node_vtable;
    baseConfig.pInputChannels = &pConfig->channels;
    baseConfig.pOutputChannels = &pConfig->channels;

    return ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pMonitorNode->baseNode);
}

void ma_monitor_node_uninit(ma_monitor_node *pMonitorNode, const ma_allocation_callbacks *pAllocationCallbacks) {
    /* The base node is always uninitialized first. */
    ma_node_uninit(pMonitorNode, pAllocationCallbacks);
}
