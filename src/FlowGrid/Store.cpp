#include "Store.h"

#include "immer/algorithm.hpp"
#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Actions.h"

TransientStore InitStore{};
Store ApplicationStore{};

namespace store {
void OnApplicationStateInitialized() {
    ApplicationStore = InitStore.persistent(); // Create the local canonical store, initially containing the full application state constructed by `State`.
    InitStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
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

#include "Helper/String.h"
#include "imgui.h"

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
