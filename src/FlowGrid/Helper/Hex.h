#pragma once

#include <format>
#include <string>
#include <string_view>

inline static std::string U32ToHex(unsigned int value, bool is_color = false) noexcept {
    if (is_color) return std::format("#{:08X}", value);
    return std::format("#{:X}", value);
}

inline static unsigned int HexToU32(string_view hex) noexcept { return std::stoul(std::string(hex.substr(1)), nullptr, 16); }
inline static bool IsHex(string_view str) noexcept { return !str.empty() && str[0] == '#'; }
