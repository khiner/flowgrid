#include "Vec2.h"

#include "imgui.h"

#include "Core/Primitive/PrimitiveActionQueuer.h"
#include "Project/ProjectContext.h"

using namespace ImGui;

Vec2::Vec2(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Component(std::move(args)), X({this, "X"}, value.first, min, max, fmt), Y({this, "Y"}, value.second, min, max, fmt) {}

Vec2::operator ImVec2() const { return {float(X), float(Y)}; }

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) ProjectContext.PrimitiveQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), std::move(value), min, max, fmt), Linked({this, "Linked"}, linked) {}

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Vec2Linked(std::move(args), std::move(value), min, max, true, fmt) {}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    bool linked = Linked;
    if (Checkbox(Linked.Name.c_str(), &linked)) ProjectContext.PrimitiveQ(Action::Vec2::ToggleLinked{Id});
    PopID();

    SameLine();

    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) {
        if (Linked) {
            const float changed_value = xy.x != float(X) ? xy.x : xy.y;
            ProjectContext.PrimitiveQ(Action::Vec2::SetAll{Id, changed_value});
        } else {
            ProjectContext.PrimitiveQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }
