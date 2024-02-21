#pragma once

#include <string>
#include <string_view>

using u32 = unsigned int;

constexpr u32 ColShiftA = 24, ColShiftR = 0, ColShiftG = 8, ColShiftB = 16;

// Copy of `IM_COL32` logic (doesn't respect `IMGUI_USE_BGRA_PACKED_COLOR` since we don't use that).
constexpr u32 Col32(u32 r, u32 g, u32 b, u32 a = 255) {
    return (a << ColShiftA) | (b << ColShiftB) | (g << ColShiftG) | (r << ColShiftR);
}
constexpr u32 HexToCol32(const std::string_view hex) {
    if (hex.empty() || hex.front() != '#' || (hex.size() != 7 && hex.size() != 9)) return Col32(255, 255, 255);

    const u32 c = std::stoul(std::string{hex.substr(1)}, nullptr, 16);
    // Assume full opacity if alpha is not specified.
    return Col32((c >> 16) & 0xFF, (c >> 8) & 0xFF, (c >> 0) & 0xFF, hex.length() == 7 ? 0xFF : ((c >> 24) & 0xFF));
}

constexpr u32 GetRed(u32 c) { return (c >> ColShiftR) & 0xFF; }
constexpr u32 GetGreen(u32 c) { return (c >> ColShiftG) & 0xFF; }
constexpr u32 GetBlue(u32 c) { return (c >> ColShiftB) & 0xFF; }
constexpr u32 GetAlpha(u32 c) { return c >> ColShiftA; }

constexpr u32 SetRed(u32 c, u32 r) { return (c & 0xFF00FFFF) | (r << 0); }
constexpr u32 SetGreen(u32 c, u32 g) { return (c & 0xFFFF00FF) | (g << 8); }
constexpr u32 SetBlue(u32 c, u32 b) { return (c & 0xFFFFFF00) | (b << 16); }
constexpr u32 SetAlpha(u32 c, u32 a) { return (c & 0x00FFFFFF) | (a << 24); }
