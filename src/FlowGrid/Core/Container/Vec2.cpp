#include "Vec2.h"

#include "imgui.h"

#include "Core/Primitive/PrimitiveActionQueuer.h"
#include "Core/Store/Store.h"

using namespace ImGui;

Vec2::Vec2(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Container(std::move(args)), Min(min), Max(max), Format(fmt), Value(std::move(value)) {
    Set(Value);
}

Vec2::~Vec2() {
    RootStore.Erase(Path / "X");
    RootStore.Erase(Path / "Y");
}

Vec2::operator ImVec2() const { return {X(), Y()}; }

void Vec2::SetX(float x) const { RootStore.Set(Path / "X", x); }
void Vec2::SetY(float y) const { RootStore.Set(Path / "Y", y); }

void Vec2::Set(const std::pair<float, float> &value) const {
    SetX(value.first);
    SetY(value.second);
}

void Vec2::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Vec2::Set &a) { Set(a.value); },
            [this](const Action::Vec2::SetX &a) { SetX(a.value); },
            [this](const Action::Vec2::SetY &a) { SetY(a.value); },
            [this](const Action::Vec2::SetAll &a) { Set({a.value, a.value}); },
            [](const Action::Vec2::ToggleLinked &) {
                throw std::runtime_error("Action::Vec2::ToggleLinked not implemented for non-linked Vec2.");
            },
        },
        action
    );
}

void Vec2::Refresh() {
    Value = {std::get<float>(RootStore.Get(Path / "X")), std::get<float>(RootStore.Get(Path / "Y"))};
}

void Vec2::SetJson(json &&j) const {
    std::pair<float, float> new_value = json::parse(std::string(std::move(j)));
    Set(std::move(new_value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
json Vec2::ToJson() const { return json(Value).dump(); }

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, Min, Max, Format, flags);
    Component::UpdateGesturing();
    if (edited) PrimitiveQ.Enqueue(Action::Vec2::Set{Path, {values.x, values.y}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

void Vec2::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    const std::string value_str = std::format("({}, {})", X(), Y());
    TreeNode(Name, false, value_str.c_str());
}

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), std::move(value), min, max, fmt), Linked(linked) {
    SetLinked(Linked);
}

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Vec2Linked(std::move(args), std::move(value), min, max, true, fmt) {}

Vec2Linked::~Vec2Linked() {
    RootStore.Erase(Path / "Linked");
}

void Vec2Linked::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Vec2::Set &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::SetX &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::SetY &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::SetAll &a) { Vec2::Apply(a); },
            [this](const Action::Vec2::ToggleLinked &) {
                SetLinked(!Linked);
                // Linking sets the max value to the min value.
                if (X() < Y()) SetY(X());
                else if (Y() < X()) SetX(Y());
            },
        },
        action
    );
}

void Vec2Linked::SetLinked(bool linked) const {
    RootStore.Set(Path / "Linked", linked);
}

void Vec2Linked::Refresh() {
    Vec2::Refresh();
    Linked = std::get<bool>(RootStore.Get(Path / "Linked"));
}

void Vec2Linked::SetJson(json &&j) const {
    std::tuple<float, float, bool> value = json::parse(std::string(std::move(j)));
    Set({std::get<0>(value), std::get<1>(value)});
    SetLinked(std::get<2>(value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
json Vec2Linked::ToJson() const {
    std::tuple<float, float, bool> value = {X(), Y(), Linked};
    return json(value).dump();
}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    bool linked = Linked;
    if (Checkbox("Linked", &linked)) PrimitiveQ.Enqueue(Action::Vec2::ToggleLinked{Path});
    PopID();

    SameLine();

    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, Min, Max, Format, flags);
    Component::UpdateGesturing();
    if (edited) {
        if (Linked) {
            const float changed_value = xy.x != X() ? xy.x : xy.y;
            PrimitiveQ.Enqueue(Action::Vec2::SetAll{Path, changed_value});
        } else {
            PrimitiveQ.Enqueue(Action::Vec2::Set{Path, {xy.x, xy.y}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }

void Vec2Linked::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    const std::string value_str = std::format("({}, {}, {})", X(), Y(), Linked ? "Linked" : "Unlinked");
    TreeNode(Name, false, value_str.c_str());
}
