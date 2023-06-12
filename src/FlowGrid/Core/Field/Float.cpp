#include "Float.h"

#include "imgui.h"

Float::Float(Stateful *parent, string_view path_leaf, string_view meta_str, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : TypedField(parent, path_leaf, meta_str, value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

// todo instead of overriding `Update` to handle ints, try ensuring floats are written to the store.
void Float::Update() {
    const Primitive PrimitiveValue = Get();
    if (std::holds_alternative<int>(PrimitiveValue)) Value = float(std::get<int>(PrimitiveValue));
    else Value = std::get<float>(PrimitiveValue);
}

using namespace ImGui;

void Float::Render() const {
    float value = Value;
    const bool edited = DragSpeed > 0 ? DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) : SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);
    UpdateGesturing();
    if (edited) Action::Primitive::Set{Path, value}.q();
    HelpMarker();
}
