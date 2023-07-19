#include "ma_data_passthrough_node.h"

#include "../ma_helper.h"

ma_data_passthrough_node_config ma_data_passthrough_node_config_init(ma_audio_buffer_ref *pDataSource) {
    ma_data_passthrough_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_node_config_init();
    config.pDataSource = pDataSource;

    return config;
}

static void ma_data_passthrough_node_process_pcm_frames(ma_node *pNode, const float **ppFramesIn, ma_uint32 *pFrameCountIn, float **ppFramesOut, ma_uint32 *pFrameCountOut) {
    ma_data_passthrough_node *pDataPassthroughNode = (ma_data_passthrough_node *)pNode;
    ma_uint32 frameCount = *pFrameCountOut;
    ma_audio_buffer_ref_set_data(pDataPassthroughNode->pDataSource, ppFramesIn[0], frameCount);

    (void)ppFramesIn;
    (void)pFrameCountIn;
}

static ma_node_vtable g_ma_data_passthrough_node_vtable =
    {
        ma_data_passthrough_node_process_pcm_frames,
        NULL, /* onGetRequiredInputFrameCount */
        1, /* 1 input bus. */
        1, /* 1 output bus. */
        MA_NODE_FLAG_PASSTHROUGH};

ma_result ma_data_passthrough_node_init(ma_node_graph *pNodeGraph, const ma_data_passthrough_node_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_data_passthrough_node *pDataPassthroughNode) {
    if (pDataPassthroughNode == NULL || pConfig == NULL) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(pDataPassthroughNode);

    ma_uint32 channels = pConfig->pDataSource->channels;
    ma_node_config baseConfig = pConfig->nodeConfig;
    baseConfig.vtable = &g_ma_data_passthrough_node_vtable; /* Explicitly set the vtable here to prevent callers from setting it incorrectly. */
    baseConfig.pInputChannels = &channels;
    baseConfig.pOutputChannels = &channels;

    ma_result result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pDataPassthroughNode->base);
    if (result != MA_SUCCESS) return result;

    pDataPassthroughNode->pDataSource = pConfig->pDataSource;

    return MA_SUCCESS;
}

void ma_data_passthrough_node_uninit(ma_data_passthrough_node *pDataPassthroughNode, const ma_allocation_callbacks *pAllocationCallbacks) {
    ma_node_uninit(&pDataPassthroughNode->base, pAllocationCallbacks);
}
