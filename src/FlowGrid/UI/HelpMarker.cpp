#include "HelpMarker.h"

#include <string>

#include "imgui.h"

using namespace ImGui;

namespace FlowGrid {
void HelpMarker(std::string_view help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(std::string(help).c_str());
        PopTextWrapPos();
        EndTooltip();
    }
}
} // namespace FlowGrid
