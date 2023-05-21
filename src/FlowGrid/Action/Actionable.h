#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../Helper/Variant.h"

// next up:
// - improve action IDs
//   - actions all get a path in addition to their name (start with all at root, but will be heirarchical soon)
//   - `ID` type generated from path, like `StateMember`:
//     `Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0))`
// - Add `Help` string (also like StateMember)
// Move all action declaration & `Apply` handling to domain files.

/**
An action is an immutable representation of a user interaction event.
Each action stores all information needed to apply the action to a `Store` instance.
An `ActionMoment` is a combination of any action (`Action::Any`) and the `TimePoint` at which the action happened.

Actions are grouped into `std::variant`s, and thus the byte size of `Action::Any` is large enough to hold its biggest type.
- For actions holding very large structured data, using a JSON string is a good approach to keep the size low
  (at the expense of losing type safety and storing the string contents in heap memory).
- Note that adding static members does not increase the size of the variant(s) it belongs to.
  (You can verify this by looking at the 'Action variant size' in the Metrics->FlowGrid window.)
*/
namespace Actionable {

struct Metadata {
    Metadata(std::string_view name);

    const std::string Name; // Human-readable name.
    // todo
    // const string PathSegment;
    // const StorePath Path;
    // const string Help, ImGuiLabel;
    // const ID Id;
};

#define Define(ActionName, ...)                          \
    struct ActionName {                                  \
        inline static const Metadata _Meta{#ActionName}; \
        inline static bool Allowed() { return true; }    \
        __VA_ARGS__;                                     \
    };

// Override `Allowed()` to return `false` if the action is not allowed in the current state.
#define DefineContextual(ActionName, ...)                \
    struct ActionName {                                  \
        inline static const Metadata _Meta{#ActionName}; \
        static bool Allowed();                           \
        __VA_ARGS__;                                     \
    };

template<typename T>
concept IsActionable = requires() {
    { T::_Meta } -> std::same_as<const Metadata &>;
};

template<typename MemberType, typename VariantType>
concept IsMember = Variant::IsMember<MemberType, VariantType>::value;

template<typename... T>
    requires(IsActionable<T> && ...)
struct ActionVariant : std::variant<T...> {
    using variant_t = std::variant<T...>; // Alias to the base variant type.
    using variant_t::variant; // Inherit the base variant's constructors.

    template<size_t I = 0>
    static auto CreateNameToIndex() {
        if constexpr (I < std::variant_size_v<variant_t>) {
            using MemberType = std::variant_alternative_t<I, variant_t>;
            auto map = CreateNameToIndex<I + 1>();
            map[MemberType::_Meta.Name] = I;
            return map;
        }
        return std::unordered_map<std::string, size_t>{};
    }

    static inline auto NameToIndex = CreateNameToIndex();

    unsigned int GetId() const { return this->index(); }

    const std::string &GetName() const {
        return Call([](auto &a) -> const std::string & { return a._Meta.Name; });
    }
    bool IsAllowed() const {
        return Call([](auto &a) { return a.Allowed(); });
    }

    template<size_t I = 0>
    static ActionVariant Create(size_t index) {
        if constexpr (I >= std::variant_size_v<variant_t>) throw std::runtime_error{"Variant index " + std::to_string(I + index) + " out of bounds"};
        else return index == 0 ? ActionVariant{std::in_place_index<I>} : Create<I + 1>(index - 1);
    }

private:
    // Call a function on the variant's active member type.
    template<typename Callable> decltype(auto) Call(Callable func) const {
        return std::visit([func](auto &action) -> decltype(auto) { return func(action); }, *this);
    }

    // Keeping this around as an example of how to define a function templated on an owned member type.
    // template<IsMember<variant_t> MemberType> static const Metadata &GetMeta() { return MemberType::_Meta; }
};

// Utility to flatten two or more `ActionVariant`s together into one variant.
// E.g. `Actionable::Combine<ActionVariant1, ActionVariant2, ActionVariant3>`
template<typename... Vars> struct Combine;
template<typename Var> struct Combine<Var> {
    using type = Var;
};
template<typename... Ts1, typename... Ts2, typename... Vars> struct Combine<ActionVariant<Ts1...>, ActionVariant<Ts2...>, Vars...> {
    using type = typename Combine<ActionVariant<Ts1..., Ts2...>, Vars...>::type;
};

/**
Get action variant index by type.
E.g. `template<typename T> constexpr size_t id = Actionable::Index<T, ActionVariant1>::value;`
*/
template<typename T, typename Var> struct Index;
template<typename T, typename... Ts> struct Index<T, ActionVariant<Ts...>> {
    static constexpr size_t value = Variant::Index<T, typename ActionVariant<Ts...>::variant_t>::value;
};
} // namespace Actionable
