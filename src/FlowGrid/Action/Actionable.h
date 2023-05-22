#pragma once

#include <array>
#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nlohmann/json.hpp"

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

    size_t GetId() const { return this->index(); }

    const std::string &GetName() const {
        return Call([](auto &a) -> const std::string & { return a._Meta.Name; });
    }
    bool IsAllowed() const {
        return Call([](auto &a) { return a.Allowed(); });
    }

    template<typename MemberType, size_t I = 0>
    struct Index {
        static constexpr size_t value = std::is_same_v<MemberType, std::variant_alternative_t<I, variant_t>> ? I : Index<MemberType, I + 1>::value;
    };
    template<typename MemberType>
    struct Index<MemberType, std::variant_size_v<variant_t>> {
        static constexpr size_t value = -1; // Type not found.
    };

    template<size_t I = 0>
    static ActionVariant Create(size_t index) {
        if constexpr (I >= std::variant_size_v<variant_t>) throw std::runtime_error{"Variant index " + std::to_string(I + index) + " out of bounds"};
        else return index == 0 ? ActionVariant{std::in_place_index<I>} : Create<I + 1>(index - 1);
    }

    // Construct a variant from its index and JSON representation.
    // Adapted for JSON from the default-ctor approach here: https://stackoverflow.com/a/60567091/780425
    template<size_t I = 0>
    static ActionVariant Create(size_t index, const nlohmann::json &j) {
        if constexpr (I >= std::variant_size_v<variant_t>) throw std::runtime_error{"Variant index " + std::to_string(I + index) + " out of bounds"};
        else return index == 0 ? j.get<std::variant_alternative_t<I, variant_t>>() : Create<I + 1>(index - 1, j);
    }

    // Serialize actions as two-element arrays, [name, value].
    // Value element can possibly be null.
    // Assumes all actions define json converters.
    inline void to_json(nlohmann::json &j) const {
        Call([&j](auto &a) { j = {a._Meta.Name, a}; });
    }
    inline static void from_json(const nlohmann::json &j, ActionVariant &value) {
        const auto name = j[0].get<std::string>();
        const auto index = NameToIndex[name];
        value = Create(index, j[1]);
    }

private:
    // Call a function on the variant's active member type.
    template<typename Callable> decltype(auto) Call(Callable func) const {
        return std::visit([func](auto &action) -> decltype(auto) { return func(action); }, *this);
    }
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
} // namespace Actionable
