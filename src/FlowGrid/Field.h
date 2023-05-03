#pragma once

#include <unordered_map>

#include "Primitive.h"
#include "StateMember.h"

using std::vector;

struct ImColor;

struct MenuItemDrawable {
    virtual void MenuItem() const = 0;
};

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Field {

struct Base : UIStateMember {
    inline static std::unordered_map<StatePath, Base *, StatePathHash> WithPath; // Find any field by its path.

    Base(StateMember *parent, string_view path_segment, string_view name_help);
    ~Base();

    virtual void Update() = 0;

protected:
    void Render() const override {}
};

struct PrimitiveBase : Base {
    PrimitiveBase(StateMember *parent, string_view path_segment, string_view name_help, Primitive value);

    Primitive Get() const; // Returns the value in the main state store.
    Primitive GetInitial() const; // Returns the value in the initialization state store.
};

template<IsPrimitive T>
struct TypedBase : PrimitiveBase {
    TypedBase(StateMember *parent, string_view path_segment, string_view name_help, T value = {})
        : PrimitiveBase(parent, path_segment, name_help, value), Value(value) {}

    operator T() const { return Value; }
    bool operator==(const T &value) const { return Value == value; }

    // Refresh the cached value based on the main store. Should be called for each affected field after a state change.
    virtual void Update() override { Value = std::get<T>(Get()); }

protected:
    T Value;
};

struct Bool : TypedBase<bool>, MenuItemDrawable {
    using TypedBase::TypedBase;

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

private:
    void Render() const override;
    void Toggle() const; // Used in draw methods.
};

using ImGuiColorEditFlags = int;

struct UInt : TypedBase<U32> {
    UInt(StateMember *parent, string_view path_segment, string_view name_help, U32 value = 0, U32 min = 0, U32 max = 100);
    UInt(StateMember *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value = 0);

    operator bool() const;
    operator int() const;
    operator ImColor() const;

    void Render(const vector<U32> &options) const;
    void ColorEdit4(ImGuiColorEditFlags flags = 0, bool allow_auto = false) const;

    const U32 Min, Max;

private:
    void Render() const override;
    string ValueName(const U32 value) const;

    const std::optional<std::function<const string(U32)>> GetName{};
};

using ImGuiSliderFlags = int;

struct Int : TypedBase<int> {
    Int(StateMember *parent, string_view path_segment, string_view name_help, int value = 0, int min = 0, int max = 100);

    operator bool() const;
    operator short() const;
    operator char() const;
    operator S8() const;

    void Render(const vector<int> &options) const;

    const int Min, Max;

private:
    void Render() const override;
};

struct Float : TypedBase<float> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Float(
        StateMember *parent, string_view path_segment, string_view name_help,
        float value = 0, float min = 0, float max = 1, const char *fmt = nullptr,
        ImGuiSliderFlags flags = 0, float drag_speed = 0
    );

    void Update() override;

    const float Min, Max, DragSpeed; // If `DragSpeed` is non-zero, this is rendered as an `ImGui::DragFloat`.
    const char *Format;
    const ImGuiSliderFlags Flags;

private:
    void Render() const override;
};

struct String : TypedBase<string> {
    String(StateMember *parent, string_view path_segment, string_view name_help, string_view value = "");

    operator bool() const;
    operator string_view() const;

    void Render(const vector<string> &options) const;

private:
    void Render() const override;
};

struct Enum : TypedBase<int>, MenuItemDrawable {
    Enum(StateMember *parent, string_view path_segment, string_view name_help, vector<string> names, int value = 0);
    Enum(StateMember *parent, string_view path_segment, string_view name_help, std::function<const string(int)> get_name, int value = 0);

    void Render(const vector<int> &options) const;
    void MenuItem() const override;

    const vector<string> Names;

private:
    void Render() const override;
    string OptionName(const int option) const;

    const std::optional<std::function<const string(int)>> GetName{};
};

// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : TypedBase<int>, MenuItemDrawable {
    struct Item {
        Item(const char *name_and_help);

        string Name, Help;
    };

    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(StateMember *parent, string_view path_segment, string_view name_help, vector<Item> items, int value = 0);

    void MenuItem() const override;

    const vector<Item> Items;

private:
    void Render() const override;
};

using Entry = std::pair<const Base &, Primitive>;
using Entries = std::vector<Entry>;

} // namespace Field
