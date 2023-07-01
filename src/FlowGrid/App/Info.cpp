#include "Info.h"

#include <format>

#include "imgui_internal.h"

#include "Audio/Faust/FaustBox.h"

using namespace ImGui;

void Info::Render() const {
    const auto hovered_id = GetHoveredID();
    if (!hovered_id) return;

    PushTextWrapPos(0);
    if (Component::ById.contains(hovered_id)) {
        const auto *member = Component::ById.at(hovered_id);
        const string help = member->Help.empty() ? std::format("No info available for \"{}\".", member->Name) : member->Help;
        TextUnformatted(help.c_str());
    } else if (IsBoxHovered(hovered_id)) {
        TextUnformatted(GetBoxInfo(hovered_id).c_str());
    }
    PopTextWrapPos();
}
