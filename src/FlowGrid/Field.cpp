#include "Field.h"

#include "imgui.h"

#include "Helper/String.h"

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
