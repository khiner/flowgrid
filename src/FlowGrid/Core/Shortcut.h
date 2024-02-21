#pragma once

#include <string>
#include <string_view>

using ImGuiKeyChord = int;

struct Shortcut {
    Shortcut(std::string_view raw);

    const std::string Raw;
    const ImGuiKeyChord KeyChord;
};
