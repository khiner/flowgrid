#include "Store.h"

#include "immer/algorithm.hpp"
#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include <range/v3/core.hpp>
#include <range/v3/view/map.hpp>

#include "Actions.h"

TransientStore InitStore{};
Store ApplicationStore{};

namespace store {
void OnApplicationStateInitialized() {
    ApplicationStore = InitStore.persistent(); // Create the global canonical store, initially containing the full application state constructed by `State`.
    InitStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
    // Ensure all store values set during initialization are reflected in cached field/collection values.
    for (auto *field : ranges::views::values(Field::Base::WithPath)) field->Update();
}

Primitive Get(const StatePath &path) { return InitStore.empty() ? AppStore.at(path) : InitStore.at(path); }
} // namespace store

// Transient modifiers
void Set(const Field::Base &field, const Primitive &value, TransientStore &store) { store.set(field.Path, value); }
void Set(const StoreEntries &values, TransientStore &store) {
    for (const auto &[path, value] : values) store.set(path, value);
}
void Set(const Field::Entries &values, TransientStore &store) {
    for (const auto &[field, value] : values) store.set(field.Path, value);
}
void Set(const StatePath &path, const vector<Primitive> &values, TransientStore &store) {
    Count i = 0;
    while (i < values.size()) {
        store.set(path / to_string(i), values[i]);
        i++;
    }

    while (store.count(path / to_string(i))) store.erase(path / to_string(i++));
}
void Set(const StatePath &path, const vector<Primitive> &data, const Count row_count, TransientStore &store) {
    assert(data.size() % row_count == 0);
    const Count col_count = data.size() / row_count;
    Count row = 0;
    while (row < row_count) {
        Count col = 0;
        while (col < col_count) {
            store.set(path / to_string(row) / to_string(col), data[row * col_count + col]);
            col++;
        }
        while (store.count(path / to_string(row) / to_string(col))) store.erase(path / to_string(row) / to_string(col++));
        row++;
    }

    while (store.count(path / to_string(row) / to_string(0))) {
        Count col = 0;
        while (store.count(path / to_string(row) / to_string(col))) store.erase(path / to_string(row) / to_string(col++));
        row++;
    }
}

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath) {
    PatchOps ops{};
    diff(
        before,
        after,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(BasePath)] = {AddOp, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(BasePath)] = {RemoveOp, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(BasePath)] = {ReplaceOp, new_element.second, old_element.second};
        }
    );

    return {ops, BasePath};
}

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include <range/v3/view/iota.hpp>

#include "Helper/String.h"
#include "UI/UI.h"
#include "UI/Widgets.h"

using namespace ImGui;

namespace Field {
Base::Base(StateMember *parent, string_view path_segment, string_view name_help) : UIStateMember(parent, path_segment, name_help) {
    WithPath[Path] = this;
}
Base::~Base() {
    WithPath.erase(Path);
}

UInt::UInt(StateMember *parent, string_view path_segment, string_view name_help, U32 value, U32 min, U32 max)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max) {}
UInt::UInt(StateMember *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value)
    : TypedBase(parent, path_segment, name_help, value), Min(0), Max(100), GetName(std::move(get_name)) {}
UInt::operator bool() const { return Value; }
UInt::operator int() const { return Value; }
UInt::operator ImColor() const { return Value; }
string UInt::ValueName(const U32 value) const { return GetName ? (*GetName)(value) : to_string(value); }

Int::Int(StateMember *parent, string_view path_segment, string_view name_help, int value, int min, int max)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max) {}
Int::operator bool() const { return Value; }
Int::operator short() const { return Value; }
Int::operator char() const { return Value; }
Int::operator S8() const { return Value; }

Float::Float(StateMember *parent, string_view path_segment, string_view name_help, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

// todo instead of overriding `Update` to handle ints, try ensuring floats are written to the store.
void Float::Update() {
    const Primitive PrimitiveValue = Get();
    if (std::holds_alternative<int>(PrimitiveValue)) Value = float(std::get<int>(PrimitiveValue));
    else Value = std::get<float>(PrimitiveValue);
}

String::String(StateMember *parent, string_view path_segment, string_view name_help, string_view value)
    : TypedBase(parent, path_segment, name_help, string(value)) {}
String::operator bool() const { return !Value.empty(); }
String::operator string_view() const { return Value; }

Enum::Enum(StateMember *parent, string_view path_segment, string_view name_help, vector<string> names, int value)
    : TypedBase(parent, path_segment, name_help, value), Names(std::move(names)) {}
Enum::Enum(StateMember *parent, string_view path_segment, string_view name_help, std::function<const string(int)> get_name, int value)
    : TypedBase(parent, path_segment, name_help, value), Names({}), GetName(std::move(get_name)) {}
string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

Flags::Flags(StateMember *parent, string_view path_segment, string_view name_help, vector<Item> items, int value)
    : TypedBase(parent, path_segment, name_help, value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto &[name, help] = StringHelper::ParseHelpText(name_and_help);
    Name = name;
    Help = help;
}
} // namespace Field

namespace Field {
PrimitiveBase::PrimitiveBase(StateMember *parent, string_view id, string_view name_help, Primitive value) : Base(parent, id, name_help) {
    Set(*this, value, InitStore);
}

Primitive PrimitiveBase::Get() const { return store::Get(Path); }

void Bool::Toggle() const { q(ToggleValue{Path}); }

void Bool::Render() const {
    bool value = Value;
    if (Checkbox(ImGuiLabel.c_str(), &value)) Toggle();
    HelpMarker();
}
bool Bool::CheckedDraw() const {
    bool value = Value;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) Toggle();
    HelpMarker();
    return toggled;
}
void Bool::MenuItem() const {
    const bool value = Value;
    HelpMarker(false);
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) Toggle();
}

void UInt::Render() const {
    U32 value = Value;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &Min, &Max, "%d");
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}
void UInt::Render(const vector<U32> &options) const {
    if (options.empty()) return;

    const U32 value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), ValueName(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(ValueName(option).c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

void UInt::ColorEdit4(ImGuiColorEditFlags flags, bool allow_auto) const {
    const Count i = std::stoi(PathSegment); // Assuming color is a member of a vector here.
    const bool is_auto = allow_auto && Value == Colors::AutoColor;
    const U32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : Value;

    PushID(ImGuiLabel.c_str());
    fg::InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
    SetItemAllowOverlap();

    // todo use auto for FG colors (link to ImGui colors)
    if (allow_auto) {
        if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
        if (Button("Auto")) q(SetValue{Path, is_auto ? mapped_value : Colors::AutoColor});
        if (!is_auto) PopStyleVar();
        SameLine();
    }

    auto value = ColorConvertU32ToFloat4(mapped_value);
    if (is_auto) BeginDisabled();
    const bool changed = ImGui::ColorEdit4("", (float *)&value, flags | ImGuiColorEditFlags_AlphaBar | (allow_auto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
    UiContext.WidgetGestured();
    if (is_auto) EndDisabled();

    SameLine(0, GetStyle().ItemInnerSpacing.x);
    TextUnformatted(Name.c_str());

    PopID();

    if (changed) q(SetValue{Path, ColorConvertFloat4ToU32(value)});
}

void Int::Render() const {
    int value = Value;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, Min, Max, "%d", ImGuiSliderFlags_None);
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}
void Int::Render(const vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), to_string(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(to_string(option).c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

void Float::Render() const {
    float value = Value;
    const bool edited = DragSpeed > 0 ? DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) : SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}

void Enum::Render() const {
    Render(ranges::views::ints(0, int(Names.size())) | ranges::to<vector>); // todo if I stick with this pattern, cache.
}
void Enum::Render(const vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = OptionName(option);
            if (Selectable(name.c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
void Enum::MenuItem() const {
    const int value = Value;
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        for (Count i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) q(SetValue{Path, int(i)});
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

void Flags::Render() const {
    const int value = Value;
    if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (Count i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) q(SetValue{Path, value ^ option_mask}); // Toggle bit
            if (!item.Help.empty()) {
                SameLine();
                fg::HelpMarker(item.Help.c_str());
            }
        }
        TreePop();
    }
    HelpMarker();
}
void Flags::MenuItem() const {
    const int value = Value;
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        for (Count i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                fg::HelpMarker(item.Help.c_str());
                SameLine();
            }
            if (ImGui::MenuItem(item.Name.c_str(), nullptr, is_selected)) q(SetValue{Path, value ^ option_mask}); // Toggle bit
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

void String::Render() const {
    const string value = Value;
    TextUnformatted(value.c_str());
}
void String::Render(const vector<string> &options) const {
    if (options.empty()) return;

    const string value = *this;
    if (BeginCombo(ImGuiLabel.c_str(), value.c_str())) {
        for (const auto &option : options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

Colors::Colors(StateMember *parent, string_view path_segment, string_view name_help, Count size, std::function<const char *(int)> get_color_name, const bool allow_auto)
    : UIStateMember(parent, path_segment, name_help), AllowAuto(allow_auto) {
    for (Count i = 0; i < size; i++) {
        new UInt(this, to_string(i), get_color_name(i)); // Adds to `Children` as a side-effect.
    }
}
Colors::~Colors() {
    const Count size = Size();
    for (int i = size - 1; i >= 0; i--) {
        delete Children[i];
    }
}

U32 Colors::ConvertFloat4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
ImVec4 Colors::ConvertU32ToFloat4(const U32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }
Count Colors::Size() const { return Children.size(); }

const UInt *Colors::At(Count i) const { return dynamic_cast<const UInt *>(Children[i]); }
U32 Colors::operator[](Count i) const { return *At(i); };
void Colors::Set(const vector<ImVec4> &values, TransientStore &transient) const {
    for (Count i = 0; i < values.size(); i++) {
        ::Set(*At(i), ConvertFloat4ToU32(values[i]), transient);
    }
}
void Colors::Set(const vector<std::pair<int, ImVec4>> &entries, TransientStore &transient) const {
    for (const auto &[i, v] : entries) {
        ::Set(*At(i), ConvertFloat4ToU32(v), transient);
    }
}

void Colors::Render() const {
    static ImGuiTextFilter filter;
    filter.Draw("Filter colors", GetFontSize() * 16);

    static ImGuiColorEditFlags flags = 0;
    if (RadioButton("Opaque", flags == ImGuiColorEditFlags_None)) flags = ImGuiColorEditFlags_None;
    SameLine();
    if (RadioButton("Alpha", flags == ImGuiColorEditFlags_AlphaPreview)) flags = ImGuiColorEditFlags_AlphaPreview;
    SameLine();
    if (RadioButton("Both", flags == ImGuiColorEditFlags_AlphaPreviewHalf)) flags = ImGuiColorEditFlags_AlphaPreviewHalf;
    SameLine();
    fg::HelpMarker("In the color list:\n"
                   "Left-click on color square to open color picker.\n"
                   "Right-click to open edit options menu.");

    BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
    PushItemWidth(-160);

    for (const auto *child : Children) {
        const auto *child_color = dynamic_cast<const UInt *>(child);
        if (filter.PassFilter(child->Name.c_str())) {
            child_color->ColorEdit4(flags, AllowAuto);
        }
    }
    if (AllowAuto) {
        Separator();
        PushTextWrapPos(0);
        Text("Colors that are set to Auto will be automatically deduced from your ImGui style or the current ImPlot colormap.\n"
             "If you want to style individual plot items, use Push/PopStyleColor around its function.");
        PopTextWrapPos();
    }

    PopItemWidth();
    EndChild();
    EndTabItem();
}

Vec2::Vec2(StateMember *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value, float min, float max, const char *fmt)
    : UIStateMember(parent, path_segment, name_help),
      X(this, "X", "", value.first, min, max), Y(this, "Y", "", value.second, min, max), Format(fmt) {}

Vec2::operator ImVec2() const { return {X, Y}; }

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, X.Min, X.Max, Format, flags);
    UiContext.WidgetGestured();
    if (edited) q(SetValues{{{X.Path, values.x}, {Y.Path, values.y}}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(StateMember *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value, float min, float max, bool linked, const char *fmt)
    : Vec2(parent, path_segment, name_help, value, min, max, fmt) {
    Set(Linked, linked, InitStore);
}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    if (Linked.CheckedDraw()) {
        // Linking sets the max value to the min value.
        if (X < Y) q(SetValue{Y.Path, X});
        else if (Y < X) q(SetValue{X.Path, Y});
    }
    PopID();
    SameLine();
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, X.Min, X.Max, Format, flags);
    UiContext.WidgetGestured();
    if (edited) {
        if (Linked) {
            const float changed_value = values.x != X ? values.x : values.y;
            q(SetValues{{{X.Path, changed_value}, {Y.Path, changed_value}}});
        } else {
            q(SetValues{{{X.Path, values.x}, {Y.Path, values.y}}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }
} // namespace Field
