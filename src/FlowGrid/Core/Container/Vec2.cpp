#include "Vec2.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "UI/Widgets.h"

using namespace ImGui;

Vec2::Vec2(ComponentArgs &&args, const std::pair<float, float> &value, float min, float max, const char *fmt)
    : Field(std::move(args)), Min(min), Max(max), Format(fmt), Value(value) {
    Set(value);
}

Vec2::operator ImVec2() const { return {X(), Y()}; }

void Vec2::Set(const std::pair<float, float> &value) const {
    const auto &[x, y] = value;
    store::Set(Path / "X", x);
    store::Set(Path / "Y", y);
}

void Vec2::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Vec2::Set &a) { Set(a.value); },
        [this](const Action::Vec2::SetX &a) { store::Set(Path / "X", a.value); },
        [this](const Action::Vec2::SetY &a) { store::Set(Path / "Y", a.value); },
        [this](const Action::Vec2::SetAll &a) { Set({a.value, a.value}); },
    );
}

void Vec2::RefreshValue() {
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

void Vec2::RenderValueTree(ValueTreeLabelMode mode, bool auto_select) const {
    Field::RenderValueTree(mode, auto_select);

    const std::string value_str = std::format("({}, {})", Value.first, Value.second);
    fg::TreeNode(Name, 0, nullptr, value_str.c_str());
}

Vec2Linked::Vec2Linked(ComponentArgs &&args, const std::pair<float, float> &value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), value, min, max, fmt) {
    Linked.Set(linked);
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
