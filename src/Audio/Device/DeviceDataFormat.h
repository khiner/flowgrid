#pragma once

#include <string>

#include <Core/Primitive/Scalar.h>

// Mirrors the anonymous struct in `ma_device_info::nativeDataFormats`, excluding `flags`.
struct DeviceDataFormat {
    // Like `ma_get_format_name(ma_format)`, but less verbose.
    static const char *GetFormatName(int format); // Convert to `ma_format`.
    std::string ToString() const;

    bool operator==(const DeviceDataFormat &other) const {
        return SampleFormat == other.SampleFormat && Channels == other.Channels && SampleRate == other.SampleRate;
    };

    int SampleFormat; // Convert to `ma_format`.
    u32 Channels;
    u32 SampleRate;
};
