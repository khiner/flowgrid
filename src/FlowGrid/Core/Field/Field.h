#pragma once

#include "Core/Stateful/Stateful.h"
#include "FieldAction.h"

// A `Field` is a drawable state-member that wraps around a primitive type.
struct Field : Stateful {
    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static std::unordered_map<StorePath, Field *, PathHash> WithPath; // Find any field by its path.

    Field(Stateful *parent, string_view path_segment, string_view name_help);
    ~Field();

    virtual void Update() = 0;
};

struct PrimitiveField : Field, Drawable {
    using Entry = std::pair<const PrimitiveField &, Primitive>;
    using Entries = std::vector<Entry>;

    PrimitiveField(Stateful *parent, string_view path_segment, string_view name_help, Primitive value);

    Primitive Get() const; // Returns the value in the main state store.

    static void Apply(const Action::Primitive &);
    static bool CanApply(const Action::Primitive &);
};

template<IsPrimitive T>
struct TypedField : PrimitiveField {
    TypedField(Stateful *parent, string_view path_segment, string_view name_help, T value = {})
        : PrimitiveField(parent, path_segment, name_help, value), Value(value) {}

    operator T() const { return Value; }
    bool operator==(const T &value) const { return Value == value; }

    // Refresh the cached value based on the main store. Should be called for each affected field after a state change.
    virtual void Update() override { Value = std::get<T>(Get()); }

protected:
    T Value;
};

namespace store {
void Set(const PrimitiveField &, const Primitive &);
void Set(const PrimitiveField::Entries &);
} // namespace store
