#include "ma_monitor_node.h"

#include "../ma_helper.h"

#include "fft_data.h"

ma_monitor_node_config ma_monitor_node_config_init(ma_uint32 channels, ma_uint32 buffer_frames) {
    ma_monitor_node_config config;
    config.node_config = ma_node_config_init(); // Input and output channels are set in ma_monitor_node_init().
    config.channels = channels;
    config.buffer_frames = buffer_frames;

    return config;
}

ma_result ma_monitor_apply_window_function(ma_monitor_node *monitor, void (*window_func)(float *, unsigned)) {
    if (monitor == nullptr) return MA_INVALID_ARGS;

    window_func(monitor->window, monitor->config.buffer_frames);

    return MA_SUCCESS;
}

static void ma_monitor_node_process_pcm_frames(ma_node *node, const float **frames_in, ma_uint32 *frame_count_in, float **frames_out, ma_uint32 *frame_count_out) {
    auto *monitor = (ma_monitor_node *)node;

    const ma_uint32 N = monitor->config.buffer_frames;
    float *working_buffer = monitor->working_buffer_index == 0 ? monitor->working_buffer_0 : monitor->working_buffer_1;
    float *buffer_write_pos = working_buffer + monitor->working_buffer_cursor;
    ma_uint32 remaining_write_frames = N - monitor->working_buffer_cursor;
    if (*frame_count_out >= remaining_write_frames) {
        ma_copy_pcm_frames(buffer_write_pos, frames_out[0], remaining_write_frames, ma_format_f32, 1);
        monitor->buffer = working_buffer;
        monitor->working_buffer_cursor = 0;
        monitor->working_buffer_index = monitor->working_buffer_index == 0 ? 1 : 0;

        // todo atomic windowing
        for (ma_uint32 i = 0; i < N; i++) {
            monitor->windowed_buffer[i] = monitor->buffer[i] * monitor->window[i];
        }

        fftwf_execute(monitor->fft->plan);

    } else {
        ma_copy_pcm_frames(buffer_write_pos, frames_out[0], *frame_count_out, ma_format_f32, 1);
        monitor->working_buffer_cursor += *frame_count_out;
    }

    (void)frame_count_in;
    (void)frames_in;
}

ma_result create_fft(ma_monitor_node *monitor, const ma_allocation_callbacks *allocation_callbacks) {
    auto *fft = (fft_data *)ma_malloc(sizeof(fft_data), allocation_callbacks);
    if (fft == nullptr) return MA_OUT_OF_MEMORY;

    ma_uint32 N = monitor->config.buffer_frames;
    fft->data = fftwf_alloc_complex(N / 2 + 1);
    if (fft->data == nullptr) {
        ma_free(fft, allocation_callbacks);
        return MA_OUT_OF_MEMORY;
    }

    fft->plan = fftwf_plan_dft_r2c_1d(N, monitor->windowed_buffer, fft->data, FFTW_MEASURE);

    monitor->fft = fft;

    return MA_SUCCESS;
}

void destroy_fft(fft_data *fft, const ma_allocation_callbacks *allocation_callbacks) {
    if (fft == nullptr) return;

    fftwf_destroy_plan(fft->plan);
    fftwf_free(fft->data);
    ma_free(fft, allocation_callbacks);
}

ma_result ma_monitor_node_init(ma_node_graph *node_graph, const ma_monitor_node_config *config, const ma_allocation_callbacks *allocation_callbacks, ma_monitor_node *monitor) {
    if (monitor == nullptr || config == nullptr) return MA_INVALID_ARGS;

    MA_ZERO_OBJECT(monitor);
    monitor->config = *config;
    ma_uint32 N = monitor->config.buffer_frames;

    monitor->working_buffer_0 = (float *)ma_malloc((size_t)(N * ma_get_bytes_per_frame(ma_format_f32, config->channels)), allocation_callbacks);
    if (monitor->working_buffer_0 == nullptr) return MA_OUT_OF_MEMORY;
    ma_silence_pcm_frames(monitor->working_buffer_0, N, ma_format_f32, config->channels);

    monitor->working_buffer_1 = (float *)ma_malloc((size_t)(N * ma_get_bytes_per_frame(ma_format_f32, config->channels)), allocation_callbacks);
    if (monitor->working_buffer_1 == nullptr) {
        ma_free(monitor->working_buffer_0, allocation_callbacks);
        return MA_OUT_OF_MEMORY;
    }
    ma_silence_pcm_frames(monitor->working_buffer_1, N, ma_format_f32, config->channels);

    monitor->buffer = monitor->working_buffer_1;

    monitor->window = (float *)ma_malloc((size_t)(N * ma_get_bytes_per_frame(ma_format_f32, 1)), allocation_callbacks);
    if (monitor->window == nullptr) {
        ma_free(monitor->working_buffer_0, allocation_callbacks);
        ma_free(monitor->working_buffer_1, allocation_callbacks);
        return MA_OUT_OF_MEMORY;
    }
    for (ma_uint32 i = 0; i < N; ++i) monitor->window[i] = 1.0; // Rectangular window by default.

    monitor->windowed_buffer = (float *)ma_malloc((size_t)(N * ma_get_bytes_per_frame(ma_format_f32, config->channels)), allocation_callbacks);
    if (monitor->windowed_buffer == nullptr) return MA_OUT_OF_MEMORY;
    ma_silence_pcm_frames(monitor->windowed_buffer, N, ma_format_f32, config->channels);

    if (ma_result result = create_fft(monitor, allocation_callbacks); result != MA_SUCCESS) {
        ma_free(monitor->working_buffer_0, allocation_callbacks);
        ma_free(monitor->working_buffer_1, allocation_callbacks);
        ma_free(monitor->window, allocation_callbacks);
        ma_free(monitor->windowed_buffer, allocation_callbacks);
        return result;
    }

    static ma_node_vtable vtable = {ma_monitor_node_process_pcm_frames, nullptr, 1, 1, MA_NODE_FLAG_PASSTHROUGH};
    ma_node_config base_config = config->node_config;
    base_config.vtable = &vtable;
    base_config.pInputChannels = &config->channels;
    base_config.pOutputChannels = &config->channels;

    return ma_node_init(node_graph, &base_config, allocation_callbacks, &monitor->base);
}

void ma_monitor_node_uninit(ma_monitor_node *monitor, const ma_allocation_callbacks *allocation_callbacks) {
    if (monitor == nullptr) return;

    ma_node_uninit(monitor, allocation_callbacks);
    destroy_fft(monitor->fft, allocation_callbacks);
    monitor->fft = nullptr;
    ma_free(monitor->buffer, allocation_callbacks);
    monitor->buffer = nullptr;
    ma_free(monitor->windowed_buffer, allocation_callbacks);
    monitor->windowed_buffer = nullptr;
}
