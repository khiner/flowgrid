#pragma once

#include <string>
#include <string_view>

struct Shortcut {
    using ImGuiModFlags = int;
    using ImGuiKey = int;
    using ImGuiFlagsAndKey = std::pair<ImGuiModFlags, ImGuiKey>;

    Shortcut(std::string_view raw);

    const std::string Raw;
    const ImGuiFlagsAndKey Parsed;
};
