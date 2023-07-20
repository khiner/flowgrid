#pragma once

#include "Core/Component.h"

// A component whose children are created/destroyed dynamically, with vector-ish semantics.
// Like all `Field`s, this wraps around an inner `Value` instance, which in this case is a `std::vector` of `std::unique_ptr<ChildType>`.
// Components typically own their children directly, declaring them as concrete instances on the stack via the `Prop` macro.
// Using `Vector` allows for runtime creation/destruction of children, and for child component types without the header having access to the full child definition.
// (It needs access to the definition for `ChildType`'s default destructor, though, since it uses `std::unique_ptr`.)
template<typename ChildType> struct Vector : Component {
    using Component::Component;

    template<typename ChildSubType, typename... Args>
        requires std::derived_from<ChildSubType, ChildType>
    void EmplaceBack(Args&&... args_without_parent) {
        Value.emplace_back(std::make_unique<ChildSubType>(this, std::forward<Args>(args_without_parent)...));
    }

    const std::unique_ptr<ChildType> &operator[](u32 i) const { return Value[i]; }

    auto begin() const { return Value.cbegin(); }
    auto end() const { return Value.cend(); }

    auto back() const { return Value.back(); }

private:
    std::vector<std::unique_ptr<ChildType>> Value;
};
