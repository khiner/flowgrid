#pragma once

#include "Stateful.h"

struct ImVec2;
struct ImColor;
using ImGuiColorEditFlags = int;
using ImGuiSliderFlags = int;

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Stateful::Field {
inline static bool IsGesturing{};

struct Base : Stateful::Base {
    inline static std::unordered_map<StorePath, Base *, StorePathHash> WithPath; // Find any field by its path.

    Base(Stateful::Base *parent, string_view path_segment, string_view name_help);
    ~Base();

    virtual void Update() = 0;
};

struct PrimitiveBase : Base, Drawable {
    PrimitiveBase(Stateful::Base *parent, string_view path_segment, string_view name_help, Primitive value);

    Primitive Get() const; // Returns the value in the main state store.
};

using Entry = std::pair<const PrimitiveBase &, Primitive>;
using Entries = std::vector<Entry>;

template<IsPrimitive T>
struct TypedBase : PrimitiveBase {
    TypedBase(Stateful::Base *parent, string_view path_segment, string_view name_help, T value = {})
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

struct UInt : TypedBase<U32> {
    UInt(Stateful::Base *parent, string_view path_segment, string_view name_help, U32 value = 0, U32 min = 0, U32 max = 100);
    UInt(Stateful::Base *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value = 0);

    operator bool() const;
    operator int() const;
    operator ImColor() const;

    void Render(const std::vector<U32> &options) const;
    void ColorEdit4(ImGuiColorEditFlags flags = 0, bool allow_auto = false) const;

    const U32 Min, Max;

    // An arbitrary transparent color is used to mark colors as "auto".
    // Using a the unique bit pattern `010101` for the RGB components so as not to confuse it with black/white-transparent.
    // Similar to ImPlot's usage of [`IMPLOT_AUTO_COL = ImVec4(0,0,0,-1)`](https://github.com/epezent/implot/blob/master/implot.h#L67).
    static constexpr U32 AutoColor = 0X00010101;

private:
    void Render() const override;
    string ValueName(const U32 value) const;

    const std::optional<std::function<const string(U32)>> GetName{};
};

struct Int : TypedBase<int> {
    Int(Stateful::Base *parent, string_view path_segment, string_view name_help, int value = 0, int min = 0, int max = 100);

    operator bool() const;
    operator short() const;
    operator char() const;
    operator S8() const;

    void Render(const std::vector<int> &options) const;

    const int Min, Max;

private:
    void Render() const override;
};

struct Float : TypedBase<float> {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Float(
        Stateful::Base *parent, string_view path_segment, string_view name_help,
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
    String(Stateful::Base *parent, string_view path_segment, string_view name_help, string_view value = "");

    operator bool() const;
    operator string_view() const;

    void Render(const std::vector<string> &options) const;

private:
    void Render() const override;
};

struct Enum : TypedBase<int>, MenuItemDrawable {
    Enum(Stateful::Base *parent, string_view path_segment, string_view name_help, std::vector<string> names, int value = 0);
    Enum(Stateful::Base *parent, string_view path_segment, string_view name_help, std::function<const string(int)> get_name, int value = 0);

    void Render(const std::vector<int> &options) const;
    void MenuItem() const override;

    const std::vector<string> Names;

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
    Flags(Stateful::Base *parent, string_view path_segment, string_view name_help, std::vector<Item> items, int value = 0);

    void MenuItem() const override;

    const std::vector<Item> Items;

private:
    void Render() const override;
};

struct Vec2 : UIStateful {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(Stateful::Base *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);

    operator ImVec2() const;

    Float X, Y;
    const char *Format;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;
};

struct Vec2Linked : Vec2 {
    using Vec2::Vec2;
    Vec2Linked(Stateful::Base *parent, string_view path_segment, string_view name_help, const std::pair<float, float> &value = {0, 0}, float min = 0, float max = 1, bool linked = true, const char *fmt = nullptr);

    Prop(Bool, Linked, true);

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};

template<IsPrimitive T>
struct Vector : Base {
    using Base::Base;

    StorePath PathAt(const Count i) const { return Path / to_string(i); }
    Count Size() const { return Value.size(); }
    T operator[](const Count i) const { return Value[i]; }

    void Set(const std::vector<T> &) const;
    void Set(const std::vector<std::pair<int, T>> &) const;

    void Update() override;

private:
    std::vector<T> Value;
};

// Vector of vectors. Inner vectors need not have the same length.
template<IsPrimitive T>
struct Vector2D : Base {
    using Base::Base;

    StorePath PathAt(const Count i, const Count j) const { return Path / to_string(i) / to_string(j); }
    Count Size() const { return Value.size(); }; // Number of outer vectors
    Count Size(Count i) const { return Value[i].size(); }; // Size of inner vector at index `i`
    T operator()(Count i, Count j) const { return Value[i][j]; }

    void Set(const std::vector<std::vector<T>> &) const;

    void Update() override;

private:
    std::vector<std::vector<T>> Value;
};

template<IsPrimitive T>
struct Matrix : Base {
    using Base::Base;

    StorePath PathAt(const Count row, const Count col) const { return Path / to_string(row) / to_string(col); }
    Count Rows() const { return RowCount; }
    Count Cols() const { return ColCount; }
    T operator()(const Count row, const Count col) const { return Data[row * ColCount + col]; }

    void Update() override;

private:
    Count RowCount, ColCount;
    std::vector<T> Data;
};
} // namespace Stateful::Field

namespace store {
void Set(const Stateful::Field::Base &, const Primitive &);
void Set(const Stateful::Field::Entries &);
} // namespace store
