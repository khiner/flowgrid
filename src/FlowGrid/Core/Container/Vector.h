#pragma once

#include <concepts>
#include <random>

#include "Core/Field/Field.h"
#include "Helper/Hex.h"

inline static std::random_device RandomDevice;
inline static std::mt19937 RandomGenerator(RandomDevice()); // Standard mersenne_twister_engine seeded with rd()
inline static std::uniform_int_distribution<u32> ChildPrefixDistribution;

template<typename T>
concept HasId = requires(T t) {
    { t.Id } -> std::same_as<const ID &>;
};

// A component whose children are created/destroyed dynamically, with vector-ish semantics.
// Like a `Field`, this wraps around an inner `Value` instance, which in this case is a `std::vector` of `std::unique_ptr<ChildType>`.
// Components typically own their children directly, declaring them as concrete instances on the stack via the `Prop` macro.
// Using `Vector` allows for runtime creation/destruction of children, and for child component types without the header having access to the full child definition.
// (It needs access to the definition for `ChildType`'s default destructor, though, since it uses `std::unique_ptr`.)
template<HasId ChildType> struct Vector : Field {
    using Field::Field;

    // Type must be constructable from `ComponentArgs`.
    template<typename ChildSubType, typename... Args>
        requires std::derived_from<ChildSubType, ChildType>
    void EmplaceBack(string_view path_segment = "", string_view meta_str = "", Args &&...other_args) {
        u32 prefix_id = ChildPrefixDistribution(RandomGenerator);
        Value.emplace_back(std::make_unique<ChildSubType>(ComponentArgs{this, path_segment, meta_str, U32ToHex(prefix_id)}, other_args...));
        CreatorByPrefixId[prefix_id] = [=]() {
            return std::make_unique<ChildSubType>(ComponentArgs{this, path_segment, meta_str, U32ToHex(prefix_id)}, other_args...);
        };
    }

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

    u32 Size() const { return Value.size(); }

    void Refresh() override;

    void EraseAt(ID id) const {
        const auto it = std::find_if(Value.begin(), Value.end(), [id](const auto &child) { return child->Id == id; });
        if (it != Value.end()) {
            it->get()->Erase();
        }
    }

    void RenderValueTree(bool annotate, bool auto_select) const override;

private:
    std::vector<std::unique_ptr<ChildType>> Value;

    // Never deleted from - accumulates throughout the lifetime of the application.
    std::unordered_map<u32, std::function<std::unique_ptr<ChildType>()>> CreatorByPrefixId;
};
