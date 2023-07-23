#pragma once

#include <format>
#include <string>
#include <string_view>

inline static std::string U32ToHex(unsigned int value) noexcept { return std::format("#{:08X}", value); }
inline static unsigned int HexToU32(string_view hex) noexcept { return std::stoul(std::string(hex.substr(1)), nullptr, 16); }
inline static bool IsHex(string_view str) noexcept { return str.size() == 9 && str[0] == '#'; }
