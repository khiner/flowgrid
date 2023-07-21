#pragma once

#include "miniaudio.h"

class dsp;

struct ma_faust_node_config {
    ma_node_config node_config;
    dsp *faust_dsp;
    ma_uint32 sample_rate;
};

ma_faust_node_config ma_faust_node_config_init(dsp *, ma_uint32 sample_rate);

struct ma_faust_node {
    ma_node_base base;
    ma_faust_node_config config;
};

ma_result ma_faust_node_init(ma_node_graph *, const ma_faust_node_config *, const ma_allocation_callbacks *, ma_faust_node *);
void ma_faust_node_uninit(ma_faust_node *, const ma_allocation_callbacks *);

ma_result ma_faust_node_set_sample_rate(ma_faust_node *, ma_uint32 sample_rate);
