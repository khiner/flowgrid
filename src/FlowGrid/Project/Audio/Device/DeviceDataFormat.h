#pragma once

#include <string>

#include "miniaudio.h"

// Mirrors the anonymous struct in `ma_device_info::nativeDataFormats`, excluding `flags`.
struct DeviceDataFormat {
    // Like `ma_get_format_name(ma_format)`, but less verbose.
    static const char *GetFormatName(ma_format);
    std::string ToString() const;

    ma_format SampleFormat;
    ma_uint32 Channels;
    ma_uint32 SampleRate;
};
