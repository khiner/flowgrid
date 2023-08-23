#include "miniaudio.h"

struct ma_channel_converter_node_config {
    ma_node_config node_config;
    ma_channel_converter_config converter_config;
};

ma_channel_converter_node_config ma_channel_converter_node_config_init(ma_uint32 in_channels, ma_uint32 out_channels);

struct ma_channel_converter_node {
    ma_node_base base;
    ma_channel_converter_node_config config;
    ma_channel_converter converter;
};

ma_result ma_channel_converter_node_init(ma_node_graph *, const ma_channel_converter_node_config *, const ma_allocation_callbacks *, ma_channel_converter_node *);
void ma_channel_converter_node_uninit(ma_channel_converter_node *, const ma_allocation_callbacks *);
