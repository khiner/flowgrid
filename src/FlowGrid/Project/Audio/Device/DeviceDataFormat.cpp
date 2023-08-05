#include "DeviceDataFormat.h"

#include <format>

const char *DeviceDataFormat::GetFormatName(ma_format format) {
    switch (format) {
        case ma_format_unknown: return "Unknown";
        case ma_format_u8: return "8-bit Unsigned Int";
        case ma_format_s16: return "16-bit Signed Int";
        case ma_format_s24: return "24-bit Signed Int";
        case ma_format_s32: return "32-bit Signed Int";
        case ma_format_f32: return "32-bit Float";
        default: return "Invalid";
    }
}

std::string DeviceDataFormat::ToString() const {
    return std::format("{} Hz | {} ch | {}", std::to_string(SampleRate), Channels, GetFormatName(SampleFormat));
}
