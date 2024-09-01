#include "Vec2.h"

#include "imgui.h"

#include "Core/Primitive/PrimitiveActionQueuer.h"

using namespace ImGui;

Vec2::Vec2(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Component(std::move(args)), X({this, "X"}, value.first, min, max, fmt), Y({this, "Y"}, value.second, min, max, fmt) {}

Vec2::operator ImVec2() const { return {float(X), float(Y)}; }

void Vec2::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Vec2::Set &a) {
                X.Set(a.value.first);
                Y.Set(a.value.second);
            },
            [this](const Action::Vec2::SetX &a) { X.Set(a.value); },
            [this](const Action::Vec2::SetY &a) { Y.Set(a.value); },
            [this](const Action::Vec2::SetAll &a) {
                X.Set(a.value);
                Y.Set(a.value);
            },
            [](const Action::Vec2::ToggleLinked &) {
                throw std::runtime_error("Action::Vec2::ToggleLinked not implemented for non-linked Vec2.");
            },
        },
        action
    );
}

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) PrimitiveQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), std::move(value), min, max, fmt), Linked({this, "Linked"}, linked) {}

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Vec2Linked(std::move(args), std::move(value), min, max, true, fmt) {}

void Vec2Linked::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Vec2::Set &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::SetX &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::SetY &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::SetAll &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::ToggleLinked &) {
                Linked.Set(!Linked);
                // Linking sets the max value to the min value.
                if (float(X) < float(Y)) Y.Set(X);
                else if (float(Y) < float(X)) X.Set(Y);
            },
        },
        action
    );
}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    bool linked = Linked;
    if (Checkbox(Linked.Name.c_str(), &linked)) PrimitiveQ(Action::Vec2::ToggleLinked{Id});
    PopID();

    SameLine();

    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) {
        if (Linked) {
            const float changed_value = xy.x != float(X) ? xy.x : xy.y;
            PrimitiveQ(Action::Vec2::SetAll{Id, changed_value});
        } else {
            PrimitiveQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }
