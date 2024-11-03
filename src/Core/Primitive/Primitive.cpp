#include "Primitive.h"

#include "Core/Store/Store.h"
#include "PrimitiveActionQueuer.h"
#include "Project/ProjectContext.h"

template<typename T> bool Primitive<T>::Exists() const { return S.Count<T>(Id); }
template<typename T> T Primitive<T>::Get() const { return S.Get<T>(Id); }

template<typename T> json Primitive<T>::ToJson() const { return Value; }
template<typename T> void Primitive<T>::SetJson(json &&j) const { Set(std::move(j)); }

template<typename T> void Primitive<T>::Set(const T &value) const { _S.Set(Id, value); }
template<typename T> void Primitive<T>::Set(T &&value) const { _S.Set(Id, std::move(value)); }
template<typename T> void Primitive<T>::Erase() const { _S.Erase<T>(Id); }

template<typename T> void Primitive<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();
    TreeNode(Name, false, std::format("{}", Value).c_str());
}

template<> void Primitive<u32>::IssueSet(const u32 &value) const { Ctx.PrimitiveQ(Action::Primitive::UInt::Set{Id, value}); };
template<> void Primitive<s32>::IssueSet(const s32 &value) const { Ctx.PrimitiveQ(Action::Primitive::Int::Set{Id, value}); };
template<> void Primitive<float>::IssueSet(const float &value) const { Ctx.PrimitiveQ(Action::Primitive::Float::Set{Id, value}); };
template<> void Primitive<std::string>::IssueSet(const std::string &value) const { Ctx.PrimitiveQ(Action::Primitive::String::Set{Id, value}); };

// Explicit instantiations.
template struct Primitive<bool>;
template struct Primitive<int>;
template struct Primitive<u32>;
template struct Primitive<float>;
template struct Primitive<std::string>;

/*** Implementations ***/

#include "imgui_internal.h"

#include "Bool.h"
#include "Core/UI/HelpMarker.h"

using namespace ImGui;

void Bool::IssueToggle() const { Ctx.PrimitiveQ(Action::Primitive::Bool::Toggle{Id}); }

void Bool::Render(std::string_view label) const {
    if (bool value = Value; Checkbox(label.data(), &value)) IssueToggle();
    HelpMarker();
}

void Bool::Render() const {
    Render(ImGuiLabel);
}

bool Bool::CheckedDraw() const {
    bool value = Value;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) IssueToggle();
    HelpMarker();
    return toggled;
}

void Bool::MenuItem() const {
    HelpMarker(false);
    if (const bool value = Value; ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) IssueToggle();
}

#include "Enum.h"

#include <ranges>

using std::ranges::to;

Enum::Enum(ComponentArgs &&args, std::vector<std::string> names, int value)
    : Primitive(std::move(args), value), Names(std::move(names)) {}
Enum::Enum(ComponentArgs &&args, std::function<std::string(int)> get_name, int value)
    : Primitive(std::move(args), value), Names({}), GetName(std::move(get_name)) {}

std::string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

void Enum::Render() const {
    Render(std::views::iota(0, int(Names.size())) | to<std::vector>()); // todo if I stick with this pattern, cache.
}
void Enum::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    if (const int value = Value; BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = OptionName(option);
            if (Selectable(name.c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
void Enum::MenuItem() const {
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        const int value = Value;
        for (u32 i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) IssueSet(int(i));
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

#include "Flags.h"

Flags::Flags(ComponentArgs &&args, std::vector<Item> items, int value)
    : Primitive(std::move(args), value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto meta = HelpInfo::Parse(name_and_help);
    Name = meta.Name;
    Help = meta.Help;
}

void Flags::Render() const {
    if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        const int value = Value;
        for (u32 i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) IssueSet(value ^ option_mask); // Toggle bit
            if (!item.Help.empty()) {
                SameLine();
                flowgrid::HelpMarker(item.Help.c_str());
            }
        }
        TreePop();
    }
    HelpMarker();
}
void Flags::MenuItem() const {
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        const int value = Value;
        for (u32 i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                flowgrid::HelpMarker(item.Help);
                SameLine();
            }
            if (ImGui::MenuItem(item.Name.c_str(), nullptr, is_selected)) IssueSet(value ^ option_mask); // Toggle bit
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

#include "Float.h"

Float::Float(ComponentArgs &&args, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : Primitive(std::move(args), value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

void Float::Render() const {
    float value = Value;
    const bool edited = DragSpeed > 0 ?
        DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) :
        SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);

    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}

#include "ID.h"

ID GenerateId(ID parent_id, ID child_id) { return ImHashData(&child_id, sizeof(child_id), parent_id); }
ID GenerateId(ID parent_id, std::string_view child_id) { return ImHashStr(child_id.data(), 0, parent_id); }

#include "Int.h"

Int::Int(ComponentArgs &&args, int value, int min, int max)
    : Primitive(std::move(args), value), Min(min), Max(max) {}

void Int::Render() const {
    int value = Value;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, Min, Max, "%d", ImGuiSliderFlags_None);
    UpdateGesturing();
    if (edited) IssueSet(value);
    HelpMarker();
}
void Int::Render(const std::vector<int> &options) const {
    if (options.empty()) return;

    if (const int value = Value; BeginCombo(ImGuiLabel.c_str(), std::to_string(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(std::to_string(option).c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

#include "UInt.h"

UInt::UInt(ComponentArgs &&args, u32 value, u32 min, u32 max)
    : Primitive(std::move(args), value), Min(min), Max(max) {}
UInt::UInt(ComponentArgs &&args, std::function<const std::string(u32)> get_name, u32 value)
    : Primitive(std::move(args), value), Min(0), Max(100), GetName(std::move(get_name)) {}

UInt::operator ImColor() const { return Value; }

std::string UInt::ValueName(u32 value) const { return GetName ? (*GetName)(value) : std::to_string(value); }

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

#include "String.h"

String::String(ComponentArgs &&args, std::string_view value) : Primitive(std::move(args), std::string(value)) {}
String::String(ComponentArgs &&args, fs::path value) : Primitive(std::move(args), std::string(value)) {}

void String::Render() const { TextUnformatted(Value); }

void String::Render(const std::vector<std::string> &options) const {
    if (options.empty()) return;

    if (const std::string value = *this; BeginCombo(ImGuiLabel.c_str(), value.c_str())) {
        for (const auto &option : options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) IssueSet(option);
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
