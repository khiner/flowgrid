#pragma once

#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
    ma_node_config node_config;
    ma_uint32 channels;
    ma_uint32 sample_rate;
    ma_uint32 buffer_frames;
} ma_monitor_node_config;

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 sample_rate, ma_uint32 buffer_frames);

typedef struct fft_data fft_data; // Forward-declare to avoid including fftw header. Include `fft_data.h` for complete definition.

typedef struct
{
    ma_node_base base_node;
    ma_monitor_node_config config;
    fft_data *fft;
    // Buffers are guaranteed to be of size `config.buffer_frames * config.channels` if initialized successfully.
    // `buffer` is the raw buffer, `window` holds the window function data, and `windowed_buffer` is the buffer after applying the window function.
    float *buffer;
    float *window;
    float *windowed_buffer;
} ma_monitor_node;

ma_result ma_monitor_node_init(ma_node_graph *, const ma_monitor_node_config *, const ma_allocation_callbacks *, ma_monitor_node *);
void ma_monitor_node_uninit(ma_monitor_node *, const ma_allocation_callbacks *);

ma_result ma_monitor_set_sample_rate(ma_monitor_node *, ma_uint32 sample_rate);

#ifdef __cplusplus
}
#endif
