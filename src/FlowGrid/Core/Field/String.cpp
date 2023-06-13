#include "String.h"

#include "imgui.h"

String::String(ComponentArgs &&args, string_view value) : TypedField(std::move(args), string(value)) {}

using namespace ImGui;

void String::Render() const {
    const string value = Value;
    TextUnformatted(value.c_str());
}
void String::Render(const std::vector<string> &options) const {
    if (options.empty()) return;

    const string value = *this;
    if (BeginCombo(ImGuiLabel.c_str(), value.c_str())) {
        for (const auto &option : options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) Action::Primitive::Set{Path, option}.q();
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
