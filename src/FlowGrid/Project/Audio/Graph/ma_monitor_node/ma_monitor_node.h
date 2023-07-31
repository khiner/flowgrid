#pragma once

#include "miniaudio.h"

struct ma_monitor_node_config {
    ma_node_config node_config;
    ma_uint32 channels;
    ma_uint32 buffer_frames;
};

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 buffer_frames);

struct fft_data; // Forward-declare to avoid including fftw header. Include `fft_data.h` for complete definition.

struct ma_monitor_node {
    ma_node_base base;
    ma_monitor_node_config config;
    fft_data *fft;
    // Buffers are guaranteed to be of size `config.buffer_frames * config.channels` if initialized successfully.
    // `buffer` always points to a full buffer, using the following double-buffering scheme:
    // * `buffer` initially points to (empty) `working_buffer_1` as `working_buffer_0` is filled up.
    // * Once `working_buffer_0` is filled up, `buffer` points to `working_buffer_0` and `working_buffer_1` starts to fill.
    // * Once `working_buffer_1` is filled up, `buffer` points to `working_buffer_1` and `working_buffer_0` starts to fill, etc.
    // At any point, the current working buffer has `working_buffer_cursor` frames written to it.
    ma_uint16 working_buffer_cursor{0};
    ma_uint8 working_buffer_index{0}; // 0 or 1.
    float *working_buffer_0;
    float *working_buffer_1;
    float *buffer; // Pointer to a full buffer (either `working_buffer_1` or `working_buffer_2`).
    float *window; // The window function frames.
    float *windowed_buffer; // The buffer after applying the window function.
};

ma_result ma_monitor_node_init(ma_node_graph *, const ma_monitor_node_config *, const ma_allocation_callbacks *, ma_monitor_node *);
void ma_monitor_node_uninit(ma_monitor_node *, const ma_allocation_callbacks *);

ma_result ma_monitor_apply_window_function(ma_monitor_node *, void (*window_func)(float *, unsigned));
