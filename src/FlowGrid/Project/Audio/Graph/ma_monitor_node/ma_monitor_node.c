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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 sample_rate, ma_uint32 buffer_frames) {
    ma_monitor_node_config config;
    MA_ZERO_OBJECT(&config);
    config.node_config = ma_node_config_init(); // Input and output channels are set in ma_monitor_node_init().
    config.channels = channels;
    config.sample_rate = sample_rate;
    config.buffer_frames = buffer_frames;

    return config;
}

static void ma_monitor_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    (void)frame_count_in;
    (void)frames_in;
    ma_monitor_node *monitor_node = (ma_monitor_node *)node;
    ma_copy_pcm_frames(monitor_node->buffer, frames_out[0], MIN(*frame_count_out, monitor_node->buffer_frames), ma_format_f32, 1);
    fftwf_execute(monitor_node->fft->plan);
}

static ma_node_vtable g_ma_monitor_node_vtable = {ma_monitor_node_process_pcm_frames, NULL, 1, 1, MA_NODE_FLAG_PASSTHROUGH};

ma_result create_fft(ma_monitor_node *monitor_node, const ma_allocation_callbacks *allocation_callbacks) {
    fft_data *fft = ma_malloc(sizeof(fft_data), allocation_callbacks);
    if (fft == NULL) return MA_OUT_OF_MEMORY;

    ma_uint32 N = monitor_node->buffer_frames;
    fft->data = fftwf_alloc_complex(N / 2 + 1);
    if (fft->data == NULL) {
        ma_free(fft, allocation_callbacks);
        return MA_OUT_OF_MEMORY;
    }

    fft->plan = fftwf_plan_dft_r2c_1d(N, monitor_node->buffer, fft->data, FFTW_MEASURE);
    fft->N = N;

    monitor_node->fft = fft;

    return MA_SUCCESS;
}

void destroy_fft(fft_data *fft, const ma_allocation_callbacks *allocation_callbacks) {
    if (fft == NULL) return;

    fftwf_destroy_plan(fft->plan);
    fftwf_free(fft->data);
    ma_free(fft, allocation_callbacks);
}

ma_result ma_monitor_node_init(ma_node_graph *node_graph, const ma_monitor_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_monitor_node *monitor_node) {
    if (monitor_node == NULL) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(monitor_node);

    if (config == NULL) return MA_INVALID_ARGS;

    monitor_node->buffer_frames = config->buffer_frames;
    monitor_node->buffer = (float *)ma_malloc((size_t)(monitor_node->buffer_frames * ma_get_bytes_per_frame(ma_format_f32, config->channels)), allocation_callbacks);
    if (monitor_node->buffer == NULL) return MA_OUT_OF_MEMORY;

    ma_silence_pcm_frames(monitor_node->buffer, monitor_node->buffer_frames, ma_format_f32, config->channels);

    int result = create_fft(monitor_node, allocation_callbacks);
    if (result != MA_SUCCESS) {
        ma_free(monitor_node->buffer, allocation_callbacks);
        return result;
    }

    ma_node_config base_config;
    base_config = config->node_config;
    base_config.vtable = &g_ma_monitor_node_vtable;
    base_config.pInputChannels = &config->channels;
    base_config.pOutputChannels = &config->channels;

    return ma_node_init(node_graph, &base_config, allocation_callbacks, &monitor_node->base_node);
}

void ma_monitor_node_uninit(ma_monitor_node *pMonitorNode, const ma_allocation_callbacks *pAllocationCallbacks) {
    destroy_fft(pMonitorNode->fft, pAllocationCallbacks);
    pMonitorNode->fft = NULL;
    /* The base node is always uninitialized first. */
    ma_node_uninit(pMonitorNode, pAllocationCallbacks);
}
