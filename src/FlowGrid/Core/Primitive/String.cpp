#include "String.h"

#include "imgui.h"

using std::string, std::string_view;

String::String(ComponentArgs &&args, string_view value) : Primitive(std::move(args), string(value)) {}
String::String(ComponentArgs &&args, fs::path value) : Primitive(std::move(args), string(value)) {}

using namespace ImGui;

void String::Render() const {
    TextUnformatted(Value.c_str());
}

void String::Render(const std::vector<string> &options) const {
    if (options.empty()) return;

    if (const string value = *this; BeginCombo(ImGuiLabel.c_str(), value.c_str())) {
        for (const auto &option : options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
