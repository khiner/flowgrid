#include "Shortcut.h"

#include <__ranges/drop_view.h>
#include <__ranges/reverse_view.h>
#include <unordered_map>

#include "imgui.h"

#include "Helper/String.h"

// Handles any number of mods, followed by a single non-mod character.
// Example: 'shift+cmd+s'
// **Case-sensitive. `shortcut` must be lowercase.**
static ImGuiKeyChord Parse(string_view shortcut) {
    static const std::unordered_map<string, ImGuiModFlags> ModKeys{
        {"shift", ImGuiModFlags_Shift},
        {"ctrl", ImGuiModFlags_Ctrl},
        {"alt", ImGuiModFlags_Alt},
        {"cmd", ImGuiModFlags_Super},
    };

    const std::vector<string> tokens = StringHelper::Split(string(shortcut), "+");
    if (tokens.empty()) throw std::runtime_error("Shortcut cannot be empty.");

    const string command = tokens.back();
    if (command.length() != 1) throw std::runtime_error("Shortcut command must be a single character.");

    const auto key = ImGuiKey(command[0] - 'a' + ImGuiKey_A);
    ImGuiModFlags mod_flags = ImGuiModFlags_None;
    for (const auto &token : std::ranges::views::reverse(tokens) | std::ranges::views::drop(1)) {
        mod_flags |= ModKeys.at(token);
    }

    return mod_flags | key;
}

Shortcut::Shortcut(string_view raw) : Raw(raw), KeyChord(Parse(Raw)) {}
