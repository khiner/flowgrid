#include "UInt.h"

#include "imgui.h"

UInt::UInt(ComponentArgs &&args, U32 value, U32 min, U32 max)
    : PrimitiveField(std::move(args), value), Min(min), Max(max) {}
UInt::UInt(ComponentArgs &&args, std::function<const string(U32)> get_name, U32 value)
    : PrimitiveField(std::move(args), value), Min(0), Max(100), GetName(std::move(get_name)) {}

UInt::operator ImColor() const { return Value; }

void UInt::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Primitive::UInt::Set &a) { Set(a.value); },
    );
}

string UInt::ValueName(const U32 value) const { return GetName ? (*GetName)(value) : to_string(value); }

using namespace ImGui;

void UInt::Render() const {
    U32 value = Value;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &Min, &Max, "%d");
    UpdateGesturing();
    if (edited) Action::Primitive::UInt::Set{Path, value}.q();
    HelpMarker();
}
void UInt::Render(const std::vector<U32> &options) const {
    if (options.empty()) return;

    const U32 value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), ValueName(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(ValueName(option).c_str(), is_selected)) Action::Primitive::UInt::Set{Path, option}.q();
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
