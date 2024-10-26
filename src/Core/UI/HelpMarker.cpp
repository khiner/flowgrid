#include "HelpMarker.h"

#include <string>

#include "imgui.h"

using namespace ImGui;

namespace flowgrid {
void HelpMarker(std::string_view help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(help.data());
        PopTextWrapPos();
        EndTooltip();
    }
}
} // namespace flowgrid
