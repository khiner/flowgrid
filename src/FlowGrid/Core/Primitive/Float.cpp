#include "Float.h"

#include "imgui.h"

Float::Float(ComponentArgs &&args, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : Primitive(std::move(args), value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

using namespace ImGui;

void Float::Render() const {
    float value = Value;
    const bool edited = DragSpeed > 0 ?
        DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) :
        SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);

    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}
