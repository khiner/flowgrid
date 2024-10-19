#pragma once

#include "miniaudio.h"

class dsp;

struct ma_faust_node_config {
    ma_node_config node_config;
    dsp *faust_dsp;
    ma_uint32 sample_rate;
    ma_uint32 buffer_frames;
};

ma_faust_node_config ma_faust_node_config_init(dsp *, ma_uint32 sample_rate, ma_uint32 buffer_frames);

struct ma_faust_node {
    ma_node_base base;
    ma_faust_node_config config;
    // These deinterleaved buffers are only created if the respective direction of the Faust node is multi-channel.
    float **in_buffer;
    float **out_buffer;
};

ma_result ma_faust_node_init(ma_node_graph *, const ma_faust_node_config *, const ma_allocation_callbacks *, ma_faust_node *);
void ma_faust_node_uninit(ma_faust_node *, const ma_allocation_callbacks *);

ma_uint32 ma_faust_dsp_get_in_channels(dsp *);
ma_uint32 ma_faust_dsp_get_out_channels(dsp *);
ma_uint32 ma_faust_node_get_in_channels(ma_faust_node *);
ma_uint32 ma_faust_node_get_out_channels(ma_faust_node *);

ma_uint32 ma_faust_node_get_sample_rate(ma_faust_node *);
dsp *ma_faust_node_get_dsp(ma_faust_node *);

ma_result ma_faust_node_set_sample_rate(ma_faust_node *, ma_uint32 sample_rate);
ma_result ma_faust_node_set_dsp(ma_faust_node *, dsp *);
