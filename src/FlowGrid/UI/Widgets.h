#pragma once

struct ImVec2;

namespace FlowGrid {}

enum InteractionFlags_ {
    InteractionFlags_None = 0,
    InteractionFlags_Hovered = 1 << 0,
    InteractionFlags_Held = 1 << 1,
    InteractionFlags_Clicked = 1 << 2,
};
using InteractionFlags = int;

namespace FlowGrid {
InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id); // Basically `ImGui::InvisibleButton`, but supporting hover/held testing.
}
