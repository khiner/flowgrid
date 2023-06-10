#include "Shortcut.h"

#include <range/v3/view/drop.hpp>
#include <range/v3/view/reverse.hpp>
#include <unordered_map>

#include "imgui.h"

#include "Helper/String.h"

using std::string;

// Handles any number of mods, followed by a single non-mod character.
// Example: 'shift+cmd+s'
// **Case-sensitive. `shortcut` must be lowercase.**
static Shortcut::ImGuiFlagsAndKey Parse(const string &shortcut) {
    const static std::unordered_map<string, ImGuiModFlags> ModKeys{
        {"shift", ImGuiModFlags_Shift},
        {"ctrl", ImGuiModFlags_Ctrl},
        {"alt", ImGuiModFlags_Alt},
        {"cmd", ImGuiModFlags_Super},
    };

    const std::vector<string> tokens = StringHelper::Split(shortcut, "+");
    if (tokens.empty()) throw std::runtime_error("Shortcut cannot be empty.");

    const string command = tokens.back();
    if (command.length() != 1) throw std::runtime_error("Shortcut command must be a single character.");

    const auto key = ImGuiKey(command[0] - 'a' + ImGuiKey_A);
    ImGuiModFlags mod_flags = ImGuiModFlags_None;
    for (const auto &token : ranges::views::reverse(tokens) | ranges::views::drop(1)) {
        mod_flags |= ModKeys.at(token);
    }

    return {mod_flags, key};
}

Shortcut::Shortcut(std::string_view raw) : Raw(raw), Parsed(Parse(Raw)) {}
