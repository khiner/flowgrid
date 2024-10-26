#pragma once

#include <string_view>

// Similar to `imgui_demo.cpp`'s `HelpMarker`.
namespace flowgrid {
void HelpMarker(std::string_view);
} // namespace flowgrid

namespace fg = flowgrid;
