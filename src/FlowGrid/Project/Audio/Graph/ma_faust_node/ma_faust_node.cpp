#include "ma_faust_node.h"

#include "../ma_helper.h"

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#include "faust/dsp/dsp.h"

ma_faust_node_config ma_faust_node_config_init(dsp *faust_dsp, ma_uint32 sample_rate, ma_uint32 buffer_frames) {
    ma_faust_node_config config;
    config.node_config = ma_node_config_init();
    config.faust_dsp = faust_dsp;
    config.sample_rate = sample_rate;
    config.buffer_frames = buffer_frames;

    return config;
}

dsp *ma_faust_node_get_dsp(ma_faust_node *faust_node) { return faust_node ? faust_node->config.faust_dsp : nullptr; }
ma_uint32 ma_faust_dsp_get_in_channels(dsp *faust_dsp) { return faust_dsp ? faust_dsp->getNumInputs() : 1; }
ma_uint32 ma_faust_dsp_get_out_channels(dsp *faust_dsp) { return faust_dsp ? faust_dsp->getNumOutputs() : 1; }

ma_uint32 ma_faust_node_get_in_channels(ma_faust_node *faust_node) { return ma_faust_dsp_get_in_channels(faust_node->config.faust_dsp); }
ma_uint32 ma_faust_node_get_out_channels(ma_faust_node *faust_node) { return ma_faust_dsp_get_out_channels(faust_node->config.faust_dsp); }

ma_uint32 ma_faust_node_get_sample_rate(ma_faust_node *faust_node) {
    return faust_node->config.sample_rate;
}

ma_result ma_faust_node_set_sample_rate(ma_faust_node *faust_node, ma_uint32 sample_rate) {
    if (faust_node == nullptr) return MA_INVALID_ARGS;

    if (faust_node->config.faust_dsp != nullptr) faust_node->config.faust_dsp->init(sample_rate);
    return MA_SUCCESS;
}

ma_result ma_faust_node_set_dsp(ma_faust_node *faust_node, dsp *faust_dsp) {
    if (faust_node == nullptr) return MA_INVALID_ARGS;
    // Reinitialize the node if the channel count has changed.
    if (ma_faust_node_get_in_channels(faust_node) != ma_uint32(faust_dsp->getNumInputs()) ||
        ma_faust_node_get_out_channels(faust_node) != ma_uint32(faust_dsp->getNumOutputs())) return MA_INVALID_ARGS;

    faust_node->config.faust_dsp = faust_dsp;
    return MA_SUCCESS;
}

static void ma_faust_node_process_pcm_frames(ma_node *node, const float **const_frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    auto *faust_node = (ma_faust_node *)node;

    if (!faust_node->config.faust_dsp) return;

    auto *dsp = faust_node->config.faust_dsp;
    float **frames_in = const_cast<float **>(const_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    ma_uint32 in_channels = ma_faust_node_get_in_channels(faust_node);
    ma_uint32 out_channels = ma_faust_node_get_out_channels(faust_node);

    if (in_channels <= 1 && out_channels <= 1) {
        // No multichannel.
        dsp->compute(*frame_count_out, frames_in, frames_out);
    } else if (in_channels <= 1) {
        // Multi output.
        dsp->compute(*frame_count_out, frames_in, faust_node->out_buffer);
        ma_interleave_pcm_frames(ma_format_f32, out_channels, *frame_count_out, (const void **)faust_node->out_buffer, frames_out[0]);
    } else if (out_channels <= 1) {
        // Multi input.
        ma_deinterleave_pcm_frames(ma_format_f32, in_channels, *frame_count_in, const_frames_in[0], (void **)faust_node->in_buffer);
        dsp->compute(*frame_count_out, faust_node->in_buffer, frames_out);
    } else {
        // Multi input/output.
        ma_deinterleave_pcm_frames(ma_format_f32, in_channels, *frame_count_in, const_frames_in[0], (void **)faust_node->in_buffer);
        dsp->compute(*frame_count_out, faust_node->in_buffer, faust_node->out_buffer);
        ma_interleave_pcm_frames(ma_format_f32, out_channels, *frame_count_out, (const void **)faust_node->out_buffer, frames_out[0]);
    }

    (void)frame_count_in;
}

ma_result ma_faust_node_init(ma_node_graph *node_graph, const ma_faust_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_faust_node *faust_node) {
    if (faust_node == nullptr || config == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(faust_node);
    faust_node->config = *config;

    auto *dsp = faust_node->config.faust_dsp;
    static ma_node_vtable vtable = {ma_faust_node_process_pcm_frames, nullptr, MA_NODE_BUS_COUNT_UNKNOWN, MA_NODE_BUS_COUNT_UNKNOWN, 0};
    // If dsp is not set, createa a passthrough node with 1 input and 1 output.
    static ma_node_vtable passthrough_vtable = {ma_faust_node_process_pcm_frames, nullptr, 1, 1, MA_NODE_FLAG_PASSTHROUGH};

    ma_uint32 in_channels = ma_faust_node_get_in_channels(faust_node);
    ma_uint32 out_channels = ma_faust_node_get_out_channels(faust_node);

    if (in_channels > 1 || out_channels > 1) {
        ma_uint32 N = faust_node->config.buffer_frames;

        if (in_channels > 1) {
            faust_node->in_buffer = (float **)ma_malloc(N * sizeof(float *), allocation_callbacks);
            for (ma_uint32 channel = 0; channel < in_channels; ++channel) {
                faust_node->in_buffer[channel] = (float *)ma_malloc(N * ma_get_bytes_per_frame(ma_format_f32, 1), allocation_callbacks);
                if (faust_node->in_buffer[channel] == nullptr) return MA_OUT_OF_MEMORY;
                ma_silence_pcm_frames(faust_node->in_buffer[channel], N, ma_format_f32, 1);
            }
        }
        if (out_channels > 1) {
            faust_node->out_buffer = (float **)ma_malloc(N * sizeof(float *), allocation_callbacks);
            for (ma_uint32 channel = 0; channel < out_channels; ++channel) {
                faust_node->out_buffer[channel] = (float *)ma_malloc(N * ma_get_bytes_per_frame(ma_format_f32, 1), allocation_callbacks);
                if (faust_node->out_buffer[channel] == nullptr) return MA_OUT_OF_MEMORY;
                ma_silence_pcm_frames(faust_node->out_buffer[channel], N, ma_format_f32, 1);
            }
        }
    }

    ma_node_config base_config = config->node_config;
    base_config.vtable = dsp ? &vtable : &passthrough_vtable;
    base_config.inputBusCount = ma_uint8(in_channels > 0 ? 1 : 0);
    base_config.outputBusCount = ma_uint8(out_channels > 0 ? 1 : 0);
    base_config.pInputChannels = in_channels > 0 ? &in_channels : nullptr;
    base_config.pOutputChannels = out_channels > 0 ? &out_channels : nullptr;

    if (dsp) dsp->init(faust_node->config.sample_rate);
    return ma_node_init(node_graph, &base_config, allocation_callbacks, &faust_node->base);
}

void ma_faust_node_uninit(ma_faust_node *faust_node, const ma_allocation_callbacks *allocation_callbacks) {
    ma_uint32 in_channels = ma_faust_node_get_in_channels(faust_node);
    ma_uint32 out_channels = ma_faust_node_get_out_channels(faust_node);
    if (in_channels > 1) {
        for (ma_uint32 channel = 0; channel < ma_faust_node_get_in_channels(faust_node); ++channel) {
            ma_free(faust_node->in_buffer[channel], allocation_callbacks);
        }
        ma_free(faust_node->in_buffer, allocation_callbacks);
    }
    if (out_channels > 1) {
        for (ma_uint32 channel = 0; channel < ma_faust_node_get_out_channels(faust_node); ++channel) {
            ma_free(faust_node->out_buffer[channel], allocation_callbacks);
        }
        ma_free(faust_node->out_buffer, allocation_callbacks);
    }
    ma_node_uninit(&faust_node->base, allocation_callbacks);
}
