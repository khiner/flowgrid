#include "HelpMarker.h"

#include "imgui.h"

using namespace ImGui;

namespace FlowGrid {
void HelpMarker(const char *help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(help);
        PopTextWrapPos();
        EndTooltip();
    }
}
} // namespace FlowGrid
