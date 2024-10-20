#include "miniaudio.h"

#include <memory>

struct ma_panner_node_config {
    ma_node_config node_config;
    ma_panner_config panner_config;
    ma_uint32 in_channels;
};

ma_panner_node_config ma_panner_node_config_init(ma_uint32 in_channels, ma_pan_mode mode = ma_pan_mode_balance);

struct ma_panner_node {
    ma_node_base base;
    ma_panner_node_config config;
    ma_panner panner;
    std::unique_ptr<ma_channel_converter> converter; // Used if `in_channels != 2`.
};

ma_result ma_panner_node_init(ma_node_graph *, const ma_panner_node_config *, const ma_allocation_callbacks *, ma_panner_node *);
void ma_panner_node_uninit(ma_panner_node *, const ma_allocation_callbacks *);

ma_result ma_panner_node_set_pan(ma_panner_node *, float pan);
ma_result ma_panner_node_set_mode(ma_panner_node *, ma_pan_mode);
