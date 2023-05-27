#include "Field.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include <range/v3/core.hpp>
#include <range/v3/view/iota.hpp>

#include "Core/Action/Action.h"
#include "Core/Store/Store.h"
#include "UI/UI.h"
#include "UI/Widgets.h"

using namespace ImGui;
using namespace Action;

using std::vector;

namespace Stateful::Field {
Base::Base(Stateful::Base *parent, string_view path_segment, string_view name_help)
    : Stateful::Base(parent, path_segment, name_help) {
    WithPath[Path] = this;
}
Base::~Base() {
    WithPath.erase(Path);
}

PrimitiveBase::PrimitiveBase(Stateful::Base *parent, string_view id, string_view name_help, Primitive value)
    : Base(parent, id, name_help) {
    store::Set(*this, value);
}

Primitive PrimitiveBase::Get() const { return store::Get(Path); }

UInt::UInt(Stateful::Base *parent, string_view path_segment, string_view name_help, U32 value, U32 min, U32 max)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max) {}
UInt::UInt(Stateful::Base *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value)
    : TypedBase(parent, path_segment, name_help, value), Min(0), Max(100), GetName(std::move(get_name)) {}
UInt::operator bool() const { return Value; }
UInt::operator int() const { return Value; }
UInt::operator ImColor() const { return Value; }
string UInt::ValueName(const U32 value) const { return GetName ? (*GetName)(value) : to_string(value); }

Int::Int(Stateful::Base *parent, string_view path_segment, string_view name_help, int value, int min, int max)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max) {}
Int::operator bool() const { return Value; }
Int::operator short() const { return Value; }
Int::operator char() const { return Value; }
Int::operator S8() const { return Value; }

Float::Float(Stateful::Base *parent, string_view path_segment, string_view name_help, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

// todo instead of overriding `Update` to handle ints, try ensuring floats are written to the store.
void Float::Update() {
    const Primitive PrimitiveValue = Get();
    if (std::holds_alternative<int>(PrimitiveValue)) Value = float(std::get<int>(PrimitiveValue));
    else Value = std::get<float>(PrimitiveValue);
}

String::String(Stateful::Base *parent, string_view path_segment, string_view name_help, string_view value)
    : TypedBase(parent, path_segment, name_help, string(value)) {}
String::operator bool() const { return !Value.empty(); }
String::operator string_view() const { return Value; }

Enum::Enum(Stateful::Base *parent, string_view path_segment, string_view name_help, vector<string> names, int value)
    : TypedBase(parent, path_segment, name_help, value), Names(std::move(names)) {}
Enum::Enum(Stateful::Base *parent, string_view path_segment, string_view name_help, std::function<const string(int)> get_name, int value)
    : TypedBase(parent, path_segment, name_help, value), Names({}), GetName(std::move(get_name)) {}
string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

Flags::Flags(Stateful::Base *parent, string_view path_segment, string_view name_help, vector<Item> items, int value)
    : TypedBase(parent, path_segment, name_help, value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto &[name, help] = ParseHelpText(name_and_help);
    Name = name;
    Help = help;
}

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
    const bool is_auto = allow_auto && Value == AutoColor;
    const U32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : Value;

    PushID(ImGuiLabel.c_str());
    fg::InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
    SetItemAllowOverlap();

    // todo use auto for FG colors (link to ImGui colors)
    if (allow_auto) {
        if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
        if (Button("Auto")) q(SetValue{Path, is_auto ? mapped_value : AutoColor});
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

Vec2::Vec2(Stateful::Base *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value, float min, float max, const char *fmt)
    : UIStateful(parent, path_segment, name_help),
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

Vec2Linked::Vec2Linked(Stateful::Base *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value, float min, float max, bool linked, const char *fmt)
    : Vec2(parent, path_segment, name_help, value, min, max, fmt) {
    store::Set(Linked, linked);
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

template<IsPrimitive T>
void Vector<T>::Set(const vector<T> &values) const {
    Count i = 0;
    while (i < values.size()) {
        store::Set(PathAt(i), T(values[i])); // When T is a bool, an explicit cast seems to be needed?
        i++;
    }
    while (store::CountAt(PathAt(i))) {
        store::Erase(PathAt(i));
        i++;
    }
}

template<IsPrimitive T>
void Vector<T>::Set(const vector<std::pair<int, T>> &values) const {
    for (const auto &[i, value] : values) store::Set(PathAt(i), value);
}

template<IsPrimitive T>
void Vector<T>::Update() {
    Count i = 0;
    while (store::CountAt(PathAt(i))) {
        const T value = std::get<T>(store::Get(PathAt(i)));
        if (Value.size() == i) Value.push_back(value);
        else Value[i] = value;
        i++;
    }
    Value.resize(i);
}

template<IsPrimitive T>
void Vector2D<T>::Set(const vector<vector<T>> &values) const {
    Count i = 0;
    while (i < values.size()) {
        Count j = 0;
        while (j < values[i].size()) {
            store::Set(PathAt(i, j), T(values[i][j]));
            j++;
        }
        while (store::CountAt(PathAt(i, j))) store::Erase(PathAt(i, j++));
        i++;
    }

    while (store::CountAt(PathAt(i, 0))) {
        Count j = 0;
        while (store::CountAt(PathAt(i, j))) store::Erase(PathAt(i, j++));
        i++;
    }
}

template<IsPrimitive T>
void Vector2D<T>::Update() {
    Count i = 0;
    while (store::CountAt(PathAt(i, 0))) {
        if (Value.size() == i) Value.push_back({});
        Count j = 0;
        while (store::CountAt(PathAt(i, j))) {
            const T value = std::get<T>(store::Get(PathAt(i, j)));
            if (Value[i].size() == j) Value[i].push_back(value);
            else Value[i][j] = value;
            j++;
        }
        Value[i].resize(j);
        i++;
    }
    Value.resize(i);
}

template<IsPrimitive T>
void Matrix<T>::Update() {
    Count row_count = 0, col_count = 0;
    while (store::CountAt(PathAt(row_count, 0))) { row_count++; }
    while (store::CountAt(PathAt(row_count - 1, col_count))) { col_count++; }
    RowCount = row_count;
    ColCount = col_count;
    Data.resize(RowCount * ColCount);

    for (Count row = 0; row < RowCount; row++) {
        for (Count col = 0; col < ColCount; col++) {
            Data[row * ColCount + col] = std::get<T>(store::Get(PathAt(row, col)));
        }
    }
}

// Explicit instantiations (boo).
template struct Vector<bool>;
template struct Vector<int>;
template struct Vector<U32>;
template struct Vector<float>;

template struct Vector2D<bool>;
template struct Vector2D<int>;
template struct Vector2D<U32>;
template struct Vector2D<float>;

template struct Matrix<bool>;
} // namespace Stateful::Field

namespace store {
void Set(const Stateful::Field::Base &field, const Primitive &value) { store::Set(field.Path, value); }
void Set(const Stateful::Field::Entries &values) {
    for (const auto &[field, value] : values) store::Set(field.Path, value);
}
} // namespace store
