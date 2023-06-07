#pragma once

#include "Core/Stateful/Stateful.h"
#include "FieldAction.h"

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Stateful::Field {
inline static bool IsGesturing{};
void UpdateGesturing();

struct Base : Stateful::Base {
    inline static std::unordered_map<StorePath, Base *, PathHash> WithPath; // Find any field by its path.

    Base(Stateful::Base *parent, string_view path_segment, string_view name_help);
    ~Base();

    virtual void Update() = 0;
};

struct PrimitiveBase : Base, Drawable {
    PrimitiveBase(Stateful::Base *parent, string_view path_segment, string_view name_help, Primitive value);

    Primitive Get() const; // Returns the value in the main state store.

    static void Apply(const Action::Value &);
    static bool CanApply(const Action::Value &);
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
} // namespace Stateful::Field

namespace store {
void Set(const Stateful::Field::Base &, const Primitive &);
void Set(const Stateful::Field::Entries &);
} // namespace store
