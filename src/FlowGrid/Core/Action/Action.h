#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "Core/Shortcut.h"
#include "Helper/Path.h"
#include "Helper/Variant.h"

/**
An action is an immutable representation of a user interaction event.
Each action stores all information needed to apply the action to a `Store` instance.
An `ActionMoment` is a combination of any action (`Action::Any`) and the `TimePoint` at which the action happened.

Actions are grouped into `ActionVariant`s, which wrap around `std::variant`.
Thus, the byte size of `Action::Any` is large enough to hold its biggest type.
- For actions holding very large structured data, using a JSON string is a good approach to keep the size low
  (at the expense of losing type safety and storing the string contents in heap memory).
- Note that adding static members does not increase the size of the variant(s) it belongs to.
  (You can verify this by looking at the 'Action variant size' in the Metrics->FlowGrid window.)
*/
namespace Action {

struct Metadata {
    // `meta_str` is of the format: "~{menu label}@{shortcut}" (order-independent, prefixes required)
    // Add `!` to the beginning of the string to indicate that the action should not be saved to the undo stack
    // (or added to the gesture history, or saved in a `.fga` (FlowGridAction) project).
    // This is used for actions with only non-state-updating side effects, like saving a file.
    Metadata(std::string_view path_suffix, std::string_view meta_str = "");

    const std::string PathSuffix; // E.g. "Set"
    const std::string Name; // Human-readable name. By default, derived as `PascalToSentenceCase(PathSuffix)`.
    const std::string MenuLabel; // Defaults to `Name`.
    const std::string Shortcut;

    // todo
    // const ID Id;
    // const string Help, ImGuiLabel;

private:
    struct Parsed {
        const std::string MenuLabel, Shortcut;
    };
    Parsed Parse(std::string_view meta_str);

    Metadata(std::string_view path_suffix, Parsed parsed);
};

template<typename T>
concept IsAction = requires(T t, bool b) {
    { T::_Meta } -> std::same_as<const Metadata &>;
    { T::IsSavable } -> std::same_as<const bool &>;
    { t.q(b) } -> std::same_as<void>;
};

template<IsAction T> struct IsSavable {
    static constexpr bool value = T::IsSavable;
};
template<IsAction T> struct IsNotSavable {
    static constexpr bool value = !T::IsSavable;
};

template<IsAction... T>
struct ActionVariant : std::variant<T...> {
    using variant_t = std::variant<T...>; // Alias to the base variant type.
    using variant_t::variant; // Inherit the base variant's constructors.

    // Note that even though these maps are declared to be instantiated for each `ActionVariant` type,
    // the compiler only instantiates them for the types with references to the map.
    template<size_t I = 0>
    static auto CreatePathToIndex() {
        if constexpr (I < std::variant_size_v<variant_t>) {
            using MemberType = std::variant_alternative_t<I, variant_t>;
            auto map = CreatePathToIndex<I + 1>();
            map[MemberType::GetPath()] = I;
            return map;
        }
        return std::unordered_map<fs::path, size_t, PathHash>{};
    }

    template<size_t I = 0>
    static auto CreateShortcuts() {
        if constexpr (I < std::variant_size_v<variant_t>) {
            using MemberType = std::variant_alternative_t<I, variant_t>;
            auto shortcuts = CreateShortcuts<I + 1>();
            if (!MemberType::GetShortcut().empty()) shortcuts.emplace_back(I, MemberType::GetShortcut());
            return shortcuts;
        }
        return std::vector<std::pair<size_t, Shortcut>>{};
    }

    size_t GetIndex() const { return this->index(); }

    const std::string &GetName() const {
        return Call([](auto &a) -> const std::string & { return a.GetName(); });
    }
    fs::path GetPath() const {
        return Call([](auto &a) { return a.GetPath(); });
    }
    bool IsSavable() const {
        return Call([](auto &a) { return a.IsSavable; });
    }
    void q() const {
        Call([](auto &a) { a.q(); });
    }

    /**
     Provided actions are assumed to be chronologically consecutive.

     Cases:
     * `b` can be merged into `a`: return the merged action
     * `b` cancels out `a` (e.g. two consecutive boolean toggles on the same value): return `true`
     * `b` cannot be merged into `a`: return `false`

     Only handling cases where merges can be determined from two consecutive actions.
     One could imagine cases where an idempotent cycle could be determined only from > 2 actions.
     For example, incrementing modulo N would require N consecutive increments to determine that they could all be cancelled out.
    */
    using MergeResult = std::variant<ActionVariant, bool>;
    MergeResult Merge(const ActionVariant &other) const {
        if (GetIndex() != other.GetIndex()) return false;
        return Call([&other](auto &a) {
            const auto &result = a.Merge(std::get<std::decay_t<decltype(a)>>(other));
            if (std::holds_alternative<bool>(result)) return MergeResult(std::get<bool>(result));
            return MergeResult(ActionVariant{std::get<std::decay_t<decltype(a)>>(result)});
        });
    }

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
        Call([&j](auto &a) { j = {a.GetPath(), a}; });
    }
    inline static void from_json(const nlohmann::json &j, ActionVariant &value) {
        static auto PathToIndex = CreatePathToIndex();
        const auto path = j[0].get<fs::path>();
        const auto index = PathToIndex[path];

        value = Create(index, j[1]);
    }

private:
    // Call a function on the variant's active member type.
    template<typename Callable> decltype(auto) Call(Callable func) const {
        return std::visit([func](auto &action) -> decltype(auto) { return func(action); }, *this);
    }
};

// Utility to flatten two or more `ActionVariant`s together into one variant.
// E.g. `Action::Combine<ActionVariant1, ActionVariant2, ActionVariant3>`
template<typename... Vars> struct Combine;
template<typename Var> struct Combine<Var> {
    using type = Var;
};
template<typename... Ts1, typename... Ts2, typename... Vars> struct Combine<ActionVariant<Ts1...>, ActionVariant<Ts2...>, Vars...> {
    using type = Combine<ActionVariant<Ts1..., Ts2...>, Vars...>::type;
};

// Utility to filter an `ActionVariant` by a predicate.
// E.g. `using Action::Stateful = Action::Filter<Action::IsSavable, Any>::type;`
template<template<typename> class Predicate, typename Var> struct Filter;
template<template<typename> class Predicate, typename... Types> struct Filter<Predicate, Action::ActionVariant<Types...>> {
    template<typename Type>
    using ConditionalAdd = std::conditional_t<Predicate<Type>::value, Action::ActionVariant<Type>, Action::ActionVariant<>>;
    using type = Action::Combine<ConditionalAdd<Types>...>::type;
};
} // namespace Action
