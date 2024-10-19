#include "InvisibleButton.h"

#include "imgui_internal.h"

using namespace ImGui;

namespace FlowGrid {
InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id) {
    auto *window = GetCurrentWindow();
    if (window->SkipItems) return false;

    const auto imgui_id = window->GetID(id);
    const auto size = CalcItemSize(size_arg, 0.0f, 0.0f);
    const auto &cursor = GetCursorScreenPos();
    const ImRect rect{cursor, cursor + size};
    if (!ItemAdd(rect, imgui_id)) return false;

    InteractionFlags flags = InteractionFlags_None;
    static bool hovered, held;
    if (ButtonBehavior(rect, imgui_id, &hovered, &held, ImGuiButtonFlags_AllowOverlap)) {
        flags |= InteractionFlags_Clicked;
    }
    if (hovered) flags |= InteractionFlags_Hovered;
    if (held) flags |= InteractionFlags_Held;

    return flags;
}
} // namespace FlowGrid
