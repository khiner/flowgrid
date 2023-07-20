#pragma once

#include <concepts>

#include "Core/Field/Field.h"

template<typename T>
concept HasId = requires(T t) {
    { t.Id } -> std::same_as<const ID &>;
};

// A component whose children are created/destroyed dynamically, with vector-ish semantics.
// Like a `Field`, this wraps around an inner `Value` instance, which in this case is a `std::vector` of `std::unique_ptr<ChildType>`.
// Components typically own their children directly, declaring them as concrete instances on the stack via the `Prop` macro.
// Using `Vector` allows for runtime creation/destruction of children, and for child component types without the header having access to the full child definition.
// (It needs access to the definition for `ChildType`'s default destructor, though, since it uses `std::unique_ptr`.)
// Thoughts:
// Tricky to delete and reconstruct objects based solely on the store.
// But that's exactly what we absolutely need to do.
// If this `Creators` pattern doesn't work (e.g. string_view may not be valid anymore?),
// we may need to _only_ support components that can be constructed with `ComponentArgs` values alone: (Parent/PathSegment/MetaStr),
// using strings instead of string_views.
template<HasId ChildType> struct Vector : Field {
    using Field::Field;

    // Type must be constructable from `ComponentArgs`.
    template<typename ChildSubType, typename... Args>
        requires std::derived_from<ChildSubType, ChildType>
    void EmplaceBack(string_view path_segment = "", string_view meta_str = "") {
        Value.emplace_back(std::make_unique<ChildSubType>(ComponentArgs{this, path_segment, meta_str}));
    }

    // Idea for more general args.
    // template<typename ChildSubType, typename... Args>
    //     requires std::derived_from<ChildSubType, ChildType>
    // void EmplaceBack(Args &&...args_without_parent) {
    //     Value.emplace_back(std::make_unique<ChildSubType>(this, std::forward<Args>(args_without_parent)...));
    //     // Store a lambda that captures the arguments and can create a new object.
    //     creators.emplace_back([=]() {
    //         return std::make_unique<ChildSubType>(this, args_without_parent...);
    //     });
    // }

    struct Iterator : std::vector<std::unique_ptr<ChildType>>::const_iterator {
        using Base = std::vector<std::unique_ptr<ChildType>>::const_iterator;

        Iterator(Base it) : Base(it) {}
        const ChildType *operator*() const { return Base::operator*().get(); }
        ChildType *operator*() { return Base::operator*().get(); }
    };

    Iterator begin() const { return Value.cbegin(); }
    Iterator end() const { return Value.cend(); }

    ChildType *back() const { return Value.back().get(); }
    ChildType *operator[](u32 i) const { return Value[i].get(); }

    void EraseAt(ID id) {
        auto it = std::find_if(Value.begin(), Value.end(), [id](const auto &child) { return child->Id == id; });
        if (it != Value.end()) {
            Value.erase(it);
        }
    }

    void Refresh() {}

private:
    std::vector<std::unique_ptr<ChildType>> Value;
    std::vector<std::function<std::unique_ptr<ChildType>()>> creators; // Stores the object creators
};
