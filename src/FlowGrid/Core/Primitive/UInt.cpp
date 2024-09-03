#include "UInt.h"

#include "imgui.h"

UInt::UInt(ComponentArgs &&args, u32 value, u32 min, u32 max)
    : Primitive(std::move(args), value), Min(min), Max(max) {}
UInt::UInt(ComponentArgs &&args, std::function<const std::string(u32)> get_name, u32 value)
    : Primitive(std::move(args), value), Min(0), Max(100), GetName(std::move(get_name)) {}

UInt::operator ImColor() const { return Value; }

std::string UInt::ValueName(u32 value) const { return GetName ? (*GetName)(value) : std::to_string(value); }

using namespace ImGui;

void UInt::Render() const {
    u32 value = Value;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &Min, &Max, "%d");
    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}
void UInt::Render(const std::vector<u32> &options) const {
    if (options.empty()) return;

    if (const u32 value = Value; BeginCombo(ImGuiLabel.c_str(), ValueName(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(ValueName(option).c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
