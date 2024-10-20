#pragma once

#include "miniaudio.h"

struct ma_waveform_node_config {
    ma_node_config node_config;
    ma_waveform_config waveform_config;
};

ma_waveform_node_config ma_waveform_node_config_init(ma_uint32 sample_rate, ma_waveform_type type, double frequency);

struct ma_waveform_node {
    ma_node_base base;
    ma_waveform_node_config config;
    ma_waveform waveform;
};

ma_result ma_waveform_node_init(ma_node_graph *, const ma_waveform_node_config *, const ma_allocation_callbacks *, ma_waveform_node *);
void ma_waveform_node_uninit(ma_waveform_node *, const ma_allocation_callbacks *);

ma_result ma_waveform_node_set_sample_rate(ma_waveform_node *, ma_uint32 sample_rate);
