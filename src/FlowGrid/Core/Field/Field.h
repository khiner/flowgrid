#pragma once

#include "Core/Component.h"
#include "PrimitiveAction.h"

// A `Field` is a component backed by a store value.
struct Field : Component {
    inline static bool IsGesturing{};
    static void UpdateGesturing();

    inline static std::unordered_map<StorePath, Field *, PathHash> WithPath; // Find any field by its path.

    Field(ComponentArgs &&);
    ~Field();

    virtual void Update() = 0;
};

struct PrimitiveField : Field, Drawable {
    using Entry = std::pair<const PrimitiveField &, Primitive>;
    using Entries = std::vector<Entry>;

    PrimitiveField(ComponentArgs &&, Primitive value);

    Primitive Get() const; // Returns the value in the main state store.

    static void Apply(const Action::Primitive::Any &);
    static bool CanApply(const Action::Primitive::Any &);
};

template<IsPrimitive T> struct TypedField : PrimitiveField {
    TypedField(ComponentArgs &&args, T value = {}) : PrimitiveField(std::move(args), value), Value(value) {}

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
