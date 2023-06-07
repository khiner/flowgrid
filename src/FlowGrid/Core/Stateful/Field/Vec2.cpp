#include "Vec2.h"

#include "imgui.h"

using namespace ImGui;

namespace Stateful::Field {
Vec2::Vec2(Stateful::Base *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value, float min, float max, const char *fmt)
    : UIStateful(parent, path_segment, name_help),
      X(this, "X", "", value.first, min, max), Y(this, "Y", "", value.second, min, max), Format(fmt) {}

Vec2::operator ImVec2() const { return {X, Y}; }

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, X.Min, X.Max, Format, flags);
    UpdateGesturing();
    if (edited) Action::SetValues{{{X.Path, values.x}, {Y.Path, values.y}}}.q();
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(Stateful::Base *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value, float min, float max, bool linked, const char *fmt)
    : Vec2(parent, path_segment, name_help, value, min, max, fmt) {
    store::Set(Linked, linked);
}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    if (Linked.CheckedDraw()) {
        // Linking sets the max value to the min value.
        if (X < Y) Action::SetValue{Y.Path, X}.q();
        else if (Y < X) Action::SetValue{X.Path, Y}.q();
    }
    PopID();
    SameLine();
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, X.Min, X.Max, Format, flags);
    UpdateGesturing();
    if (edited) {
        if (Linked) {
            const float changed_value = values.x != X ? values.x : values.y;
            Action::SetValues{{{X.Path, changed_value}, {Y.Path, changed_value}}}.q();
        } else {
            Action::SetValues{{{X.Path, values.x}, {Y.Path, values.y}}}.q();
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }
} // namespace Stateful::Field
