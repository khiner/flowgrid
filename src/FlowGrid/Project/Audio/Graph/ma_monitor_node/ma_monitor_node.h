#pragma once

#include "miniaudio.h"

struct ma_monitor_node_config {
    ma_node_config node_config;
    ma_uint32 channels;
    ma_uint32 sample_rate;
    ma_uint32 buffer_frames;
};

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 sample_rate, ma_uint32 buffer_frames);

struct fft_data; // Forward-declare to avoid including fftw header. Include `fft_data.h` for complete definition.

struct ma_monitor_node {
    ma_node_base base;
    ma_monitor_node_config config;
    fft_data *fft;
    // Since MA splits up callback buffers into chunks limited by `cachedDataCapInFramesPerBus`,
    // we need to keep track of how many frames we've processed so far.
    // When `processed_buffer_frame_count` reaches `config.buffer_frames`, we process the buffer.
    ma_uint16 processed_buffer_frame_count{0};
    // Buffers are guaranteed to be of size `config.buffer_frames * config.channels` if initialized successfully.
    // `buffer` is the raw buffer, `window` holds the window function data, and `windowed_buffer` is the buffer after applying the window function.
    float *buffer;
    float *window;
    float *windowed_buffer;
};

ma_result ma_monitor_node_init(ma_node_graph *, const ma_monitor_node_config *, const ma_allocation_callbacks *, ma_monitor_node *);
void ma_monitor_node_uninit(ma_monitor_node *, const ma_allocation_callbacks *);

ma_result ma_monitor_set_sample_rate(ma_monitor_node *, ma_uint32 sample_rate);
ma_result ma_monitor_apply_window_function(ma_monitor_node *, void (*window_func)(float *, unsigned));
