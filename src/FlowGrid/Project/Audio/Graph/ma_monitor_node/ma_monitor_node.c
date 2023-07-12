#include "ma_monitor_node.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fft_data.h"

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

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 sample_rate, ma_uint32 buffer_frames) {
    ma_monitor_node_config config;
    MA_ZERO_OBJECT(&config);
    config.node_config = ma_node_config_init(); // Input and output channels are set in ma_monitor_node_init().
    config.channels = channels;
    config.sample_rate = sample_rate;
    config.buffer_frames = buffer_frames;

    return config;
}

ma_result ma_monitor_set_sample_rate(ma_monitor_node* monitor, ma_uint32 sample_rate) {
    if (monitor == NULL) return MA_INVALID_ARGS;

    monitor->config.sample_rate = sample_rate;

    // Nothing else to do. This only affects frequency calculation for the UI.

    return MA_SUCCESS;
}

static void ma_monitor_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    (void)frame_count_in;
    (void)frames_in;
    ma_monitor_node *monitor = (ma_monitor_node *)node;
    assert(*frame_count_out == monitor->config.buffer_frames);
    ma_copy_pcm_frames(monitor->buffer, frames_out[0], monitor->config.buffer_frames, ma_format_f32, 1);
    fftwf_execute(monitor->fft->plan);
}

static ma_node_vtable g_ma_monitor_node_vtable = {ma_monitor_node_process_pcm_frames, NULL, 1, 1, MA_NODE_FLAG_PASSTHROUGH};

ma_result create_fft(ma_monitor_node *monitor, const ma_allocation_callbacks *allocation_callbacks) {
    fft_data *fft = ma_malloc(sizeof(fft_data), allocation_callbacks);
    if (fft == NULL) return MA_OUT_OF_MEMORY;

    ma_uint32 N = monitor->config.buffer_frames;
    fft->data = fftwf_alloc_complex(N / 2 + 1);
    if (fft->data == NULL) {
        ma_free(fft, allocation_callbacks);
        return MA_OUT_OF_MEMORY;
    }

    fft->plan = fftwf_plan_dft_r2c_1d(N, monitor->buffer, fft->data, FFTW_MEASURE);
    fft->N = N;

    monitor->fft = fft;

    return MA_SUCCESS;
}

void destroy_fft(fft_data *fft, const ma_allocation_callbacks *allocation_callbacks) {
    if (fft == NULL) return;

    fftwf_destroy_plan(fft->plan);
    fftwf_free(fft->data);
    ma_free(fft, allocation_callbacks);
}

ma_result ma_monitor_node_init(ma_node_graph *node_graph, const ma_monitor_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_monitor_node *monitor) {
    if (monitor == NULL || config == NULL) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(monitor);
    monitor->config = *config;
    ma_uint32 N = monitor->config.buffer_frames;
    monitor->buffer = (float *)ma_malloc((size_t)(N * ma_get_bytes_per_frame(ma_format_f32, config->channels)), allocation_callbacks);
    if (monitor->buffer == NULL) return MA_OUT_OF_MEMORY;

    // monitor->window_buffer = (float *)ma_malloc((size_t)(N * ma_get_bytes_per_frame(ma_format_f32, config->channels)), allocation_callbacks);
    // if (monitor->window_buffer == NULL) return MA_OUT_OF_MEMORY;

    ma_silence_pcm_frames(monitor->buffer, N, ma_format_f32, config->channels);
    // ma_silence_pcm_frames(monitor->window_buffer, N, ma_format_f32, config->channels);

    int result = create_fft(monitor, allocation_callbacks);
    if (result != MA_SUCCESS) {
        ma_free(monitor->buffer, allocation_callbacks);
        return result;
    }

    ma_node_config base_config;
    base_config = config->node_config;
    base_config.vtable = &g_ma_monitor_node_vtable;
    base_config.pInputChannels = &config->channels;
    base_config.pOutputChannels = &config->channels;

    return ma_node_init(node_graph, &base_config, allocation_callbacks, &monitor->base_node);
}

void ma_monitor_node_uninit(ma_monitor_node *pMonitorNode, const ma_allocation_callbacks *pAllocationCallbacks) {
    destroy_fft(pMonitorNode->fft, pAllocationCallbacks);
    pMonitorNode->fft = NULL;
    /* The base node is always uninitialized first. */
    ma_node_uninit(pMonitorNode, pAllocationCallbacks);
}
