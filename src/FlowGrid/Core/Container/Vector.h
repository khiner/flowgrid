#pragma once

#include <concepts>

#include "Core/Component.h"

// A component whose children are created/destroyed dynamically, with vector-ish semantics.
// Like a `Field`, this wraps around an inner `Value` instance, which in this case is a `std::vector` of `std::unique_ptr<ChildType>`.
// Components typically own their children directly, declaring them as concrete instances on the stack via the `Prop` macro.
// Using `Vector` allows for runtime creation/destruction of children, and for child component types without the header having access to the full child definition.
// (It needs access to the definition for `ChildType`'s default destructor, though, since it uses `std::unique_ptr`.)
template<typename ChildType> struct Vector : Component {
    using Component::Component;

    template<typename ChildSubType, typename... Args>
        requires std::derived_from<ChildSubType, ChildType>
    void EmplaceBack(Args &&...args_without_parent) {
        Value.emplace_back(std::make_unique<ChildSubType>(this, std::forward<Args>(args_without_parent)...));
    }

    ChildType *operator[](u32 i) const { return Value[i].get(); }

    struct Iterator : std::vector<std::unique_ptr<ChildType>>::const_iterator {
        using Base = std::vector<std::unique_ptr<ChildType>>::const_iterator;

        Iterator(Base it) : Base(it) {}
        const ChildType *operator*() const { return Base::operator*().get(); }
        ChildType *operator*() { return Base::operator*().get(); }
    };

    Iterator begin() const { return Value.cbegin(); }
    Iterator end() const { return Value.cend(); }

    ChildType *back() const { return Value.back().get(); }

private:
    std::vector<std::unique_ptr<ChildType>> Value;
};