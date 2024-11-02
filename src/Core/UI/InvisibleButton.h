#pragma once

struct ImVec2;

enum InteractionFlags_ {
    InteractionFlags_None = 0,
    InteractionFlags_Hovered = 1 << 0,
    InteractionFlags_Held = 1 << 1,
    InteractionFlags_Clicked = 1 << 2,
};
using InteractionFlags = int;

// Basically `ImGui::InvisibleButton`, but supports hover/held testing.
namespace flowgrid {
InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id);
} // namespace flowgrid
