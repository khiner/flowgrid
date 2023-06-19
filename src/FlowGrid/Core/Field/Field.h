#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Component.h"
#include "PrimitiveAction.h"

#include "FieldActionHandler.h"

using FieldValue = std::variant<Primitive, std::pair<float, float>>; // Includes float pair for `Vec2`.

// A `Field` is a component backed by a store value.
struct Field : Component {
    inline static std::vector<Field *> Instances; // All fields.
    inline static std::map<StorePath, U32> IndexForPath; // Maps store paths to field indices.

    inline static bool IsGesturing{};
    static void UpdateGesturing();

    static Field *FindByPath(const StorePath &);

    inline static FieldActionHandler ActionHandler;

    Field(ComponentArgs &&);
    ~Field();

    Field &operator=(const Field &) = delete;

    // Refresh the cached value based on the main store.
    // Should be called for each affected field after a state change to avoid stale values.
    virtual void RefreshValue() = 0;

    U32 Index; // Index in `Instances`.
};

struct ExtendedPrimitiveField : Field, Drawable {
    using Field::Field;

    using Entry = std::pair<const ExtendedPrimitiveField &, FieldValue>;
    using Entries = std::vector<Entry>;

    virtual FieldValue GetValue() const = 0; // Returns the value in the main state store.
};

struct PrimitiveField : ExtendedPrimitiveField, Actionable<Action::Primitive::Any> {
    PrimitiveField(ComponentArgs &&, Primitive value);

    void Set(const Primitive &) const;

    Primitive Get() const; // Returns the value in the main state store.
    FieldValue GetValue() const override { return Get(); };

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };
};

template<IsPrimitive T> struct TypedField : PrimitiveField {
    TypedField(ComponentArgs &&args, T value = {}) : PrimitiveField(std::move(args), value), Value(value) {}

    operator T() const { return Value; }
    bool operator==(const T &value) const { return Value == value; }

    // Non-mutating set. Only updates store. Used during action application.
    void Set(const T &) const;

    // Mutating set. Updates both store and cached value.
    // Should only be used during initialization and side-effect handling pass.
    void Set_(const T &value) {
        Set(value);
        Value = value;
    }

    // Refresh the cached value based on the main store. Should be called for each affected field after a state change.
    virtual void RefreshValue() override { Value = std::get<T>(Get()); }

protected:
    T Value;
};

namespace store {
void Set(const ExtendedPrimitiveField &, const FieldValue &);
void Set(const ExtendedPrimitiveField::Entries &);
} // namespace store
