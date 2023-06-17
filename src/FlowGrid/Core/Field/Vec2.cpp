#include "Vec2.h"

#include "imgui.h"

#include "Core/Store/Store.h"

using namespace ImGui;

Vec2::Vec2(ComponentArgs &&args, const std::pair<float, float> &value, float min, float max, const char *fmt)
    : ExtendedPrimitiveField(std::move(args)), Min(min), Max(max), Format(fmt), Value(value) {
    Apply(Action::Vec2::Set{Path, value});
}

Vec2::operator ImVec2() const { return {X(), Y()}; }

void Vec2::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Vec2::Set &a) {
            const auto &[x, y] = a.value;
            store::Set(Path / "X", x);
            store::Set(Path / "Y", y);
        },
        [this](const Action::Vec2::SetX &a) { store::Set(Path / "X", a.value); },
        [this](const Action::Vec2::SetY &a) { store::Set(Path / "Y", a.value); },
        [this](const Action::Vec2::SetAll &a) {
            store::Set(Path / "X", a.value);
            store::Set(Path / "Y", a.value);
        },
    );
}

void Vec2::Update() {
    Value = {std::get<float>(store::Get(Path / "X")), std::get<float>(store::Get(Path / "Y"))};
}

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, Min, Max, Format, flags);
    Field::UpdateGesturing();
    if (edited) Action::Vec2::Set{Path, {values.x, values.y}}.q();
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(ComponentArgs &&args, const std::pair<float, float> &value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), value, min, max, fmt) {
    store::Set(Linked, linked);
}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    if (Linked.CheckedDraw()) {
        // Linking sets the max value to the min value.
        if (X() < Y()) Action::Vec2::SetY{Path, X()}.q();
        else if (Y() < X()) Action::Vec2::SetX{Path, Y()}.q();
    }
    PopID();
    SameLine();
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, Min, Max, Format, flags);
    Field::UpdateGesturing();
    if (edited) {
        if (Linked) {
            const float changed_value = values.x != X() ? values.x : values.y;
            Action::Vec2::SetAll{Path, changed_value}.q();
        } else {
            Action::Vec2::Set{Path, {values.x, values.y}}.q();
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }
