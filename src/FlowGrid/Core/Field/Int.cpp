#include "Int.h"

#include "imgui.h"

Int::Int(Stateful *parent, string_view path_leaf, string_view meta_str, int value, int min, int max)
    : TypedField(parent, path_leaf, meta_str, value), Min(min), Max(max) {}
Int::operator bool() const { return Value; }
Int::operator short() const { return Value; }
Int::operator char() const { return Value; }
Int::operator S8() const { return Value; }

using namespace ImGui;

void Int::Render() const {
    int value = Value;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, Min, Max, "%d", ImGuiSliderFlags_None);
    UpdateGesturing();
    if (edited) Action::Primitive::Set{Path, value}.q();
    HelpMarker();
}
void Int::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), to_string(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(to_string(option).c_str(), is_selected)) Action::Primitive::Set{Path, option}.q();
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
