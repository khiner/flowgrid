#include "ma_faust_node.h"

#include "../ma_helper.h"

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#include "faust/dsp/dsp.h"

ma_faust_node_config ma_faust_node_config_init(dsp *faust_dsp, ma_uint32 sample_rate) {
    ma_faust_node_config config;
    config.node_config = ma_node_config_init();
    config.faust_dsp = faust_dsp;
    config.sample_rate = sample_rate;

    return config;
}

ma_result ma_faust_node_set_sample_rate(ma_faust_node *faust_node, ma_uint32 sample_rate) {
    if (faust_node == nullptr) return MA_INVALID_ARGS;

    if (faust_node->config.faust_dsp != nullptr) faust_node->config.faust_dsp->init(sample_rate);
    return MA_SUCCESS;
}

static void ma_faust_node_process_pcm_frames(ma_node *node, const float **const_frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    ma_faust_node *faust_node = (ma_faust_node *)node;

    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **frames_in = const_cast<float **>(const_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    if (faust_node->config.faust_dsp) faust_node->config.faust_dsp->compute(*frame_count_out, frames_in, frames_out);

    (void)frame_count_in;
}

ma_result ma_faust_node_init(ma_node_graph *node_graph, const ma_faust_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_faust_node *faust_node) {
    if (faust_node == nullptr || config == nullptr || config->faust_dsp == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(faust_node);
    faust_node->config = *config;

    faust_node->config.faust_dsp->init(faust_node->config.sample_rate);
    ma_uint32 in_channels = faust_node->config.faust_dsp->getNumInputs();
    ma_uint32 out_channels = faust_node->config.faust_dsp->getNumOutputs();
    static ma_node_vtable vtable = {ma_faust_node_process_pcm_frames, nullptr, ma_uint8(in_channels > 0 ? 1 : 0), ma_uint8(out_channels > 0 ? 1 : 0), 0};
    ma_node_config base_config = config->node_config;
    base_config.vtable = &vtable;
    base_config.pInputChannels = &in_channels;
    base_config.pOutputChannels = &out_channels;

    return ma_node_init(node_graph, &base_config, allocation_callbacks, &faust_node->base);
}

void ma_faust_node_uninit(ma_faust_node *faust_node, const ma_allocation_callbacks *allocation_callbacks) {
    ma_node_uninit(&faust_node->base, allocation_callbacks);
}
