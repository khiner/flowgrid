#pragma once

#include <format>
#include <string>
#include <string_view>

// Expecting "#RRGGBB" or "#RRGGBBAA" (lowercase or uppercase).
inline bool IsHex(std::string_view str) noexcept {
    return !str.empty() && str[0] == '#' && (str.size() == 7 || str.size() == 9);
}

inline std::string U32ToHex(unsigned int value, bool is_color = false) noexcept {
    if (is_color) return std::format("#{:08X}", value);
    return std::format("#{:X}", value);
}

inline unsigned int HexToU32(std::string_view hex) noexcept {
    if (!IsHex(hex)) return 0;
    return std::stoul(std::string(hex.substr(1)) + (hex.size() == 7 ? "FF" : ""), nullptr, 16);
}
