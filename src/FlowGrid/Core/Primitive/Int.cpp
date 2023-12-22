#include "Int.h"

#include "imgui.h"

Int::Int(ComponentArgs &&args, int value, int min, int max)
    : Primitive(std::move(args), value), Min(min), Max(max) {}

void Int::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Primitive::Int::Set &a) { Set(a.value); },
    );
}

using namespace ImGui;

void Int::Render() const {
    int value = Value;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, Min, Max, "%d", ImGuiSliderFlags_None);
    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}
void Int::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), to_string(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(to_string(option).c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
