#pragma once

#include <string>
#include <string_view>

using u32 = unsigned int;

// Copy of `IM_COL32` logic (doesn't respect `IMGUI_USE_BGRA_PACKED_COLOR` since we don't use that).
constexpr u32 Col32(u32 r, u32 g, u32 b, u32 a = 255) {
    return (a << 24) | (b << 16) | (g << 8) | (r << 0);
}
constexpr u32 HexToCol32(const std::string_view hex) {
    if (hex.empty() || hex.front() != '#' || (hex.size() != 7 && hex.size() != 9)) return Col32(255, 255, 255);

    const u32 c = std::stoul(std::string{hex.substr(1)}, nullptr, 16);
    // Assume full opacity if alpha is not specified.
    return Col32((c >> 16) & 0xFF, (c >> 8) & 0xFF, (c >> 0) & 0xFF, hex.length() == 7 ? 0xFF : ((c >> 24) & 0xFF));
}

constexpr u32 SetAlpha(u32 color, u32 a) { return (color & 0x00FFFFFF) | (a << 24); }
