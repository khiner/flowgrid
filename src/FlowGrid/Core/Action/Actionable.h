#pragma once

#include <array>
#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "Helper/Variant.h"

// next up:
// - improve action IDs
//   - actions all get a path in addition to their name (start with all at root, but will be heirarchical soon)
//   - `ID` type generated from path, like `Stateful`:
//     `Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0))`
// - Add `Help` string (also like Stateful)
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
    // `meta_str` is of the format: "~{menu label}@{shortcut}" (order-independent, prefixes required)
    // Add `!` to the beginning of the string to indicate that the action should not be saved to the undo stack
    // (or added to the gesture history, or saved in a `.fga` (FlowGridAction) project).
    // This is used for actions with only non-state-updating side effects, like saving a file.
    Metadata(std::string_view name, std::string_view meta_str = "");

    const std::string Name; // Human-readable name.
    const std::string MenuLabel; // Defaults to `Name`.
    const std::string Shortcut;

    // todo
    // const string PathSegment;
    // const StorePath Path;
    // const string Help, ImGuiLabel;
    // const ID Id;

private:
    struct Parsed {
        const std::string MenuLabel, Shortcut;
    };
    Parsed ParseMetadata(std::string_view meta_str);
    Metadata(std::string_view name, Parsed parsed);
};

#define ALLOWED_FUNCTION_0 \
    inline static bool Allowed() { return true; }

#define ALLOWED_FUNCTION_1 static bool Allowed();

// Use `Merge` for actions that can be merged with any other action of the same type.
// Use `NoMerge` for actions that cannot be merged with any other action.
// Use `CustomMerge` to override the `Merge` function with a custom implementation.
#define MergeType_NoMerge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &) const { return false; }
#define MergeType_Merge(ActionType) \
    inline std::variant<ActionType, bool> Merge(const ActionType &other) const { return other; }
#define MergeType_CustomMerge(ActionType) std::variant<ActionType, bool> Merge(const ActionType &) const;

// Pass `is_contextual = 1` and override `{ActionType}::Allowed()` to return `false` if the action is not allowed in the current state.
// Pass `is_savable = 1` to declare the action as savable (undoable, gesture history, saved in `.fga` projects).
/**
 `q` is the main action-queue method.
 Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
 This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
 Defined in `App.cpp`.
*/

#define Define(ActionType, is_savable, is_contextual, merge_type, meta_str, ...) \
    struct ActionType {                                                          \
        inline static const Metadata _Meta{#ActionType, meta_str};               \
        static constexpr bool IsSavable = is_savable;                            \
        void q(bool flush = false) const;                                        \
        static void MenuItem();                                                  \
        static const std::string &GetName() { return _Meta.Name; }               \
        static const std::string &GetMenuLabel() { return _Meta.MenuLabel; }     \
        static const std::string &GetShortcut() { return _Meta.Shortcut; }       \
        ALLOWED_FUNCTION_##is_contextual                                         \
            MergeType_##merge_type(ActionType)                                   \
                __VA_ARGS__;                                                     \
    };

template<typename T>
concept IsActionable = requires() {
    { T::_Meta } -> std::same_as<const Metadata &>;
    { T::IsSavable } -> std::same_as<const bool &>;
};

template<IsActionable T> struct IsSavable {
    static constexpr bool value = T::IsSavable;
};
template<IsActionable T> struct NonSavable {
    static constexpr bool value = !T::IsSavable;
};

template<IsActionable... T>
struct ActionVariant : std::variant<T...> {
    using variant_t = std::variant<T...>; // Alias to the base variant type.
    using variant_t::variant; // Inherit the base variant's constructors.

    // Note that even though these maps are declared to be instantiated for each `ActionVariant` type,
    // the compiler only instantiates them for the types with references to the map.
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

    template<size_t I = 0>
    static auto CreateIndexToShortcut() {
        if constexpr (I < std::variant_size_v<variant_t>) {
            using MemberType = std::variant_alternative_t<I, variant_t>;
            auto map = CreateIndexToShortcut<I + 1>();
            if (!MemberType::_Meta.Shortcut.empty()) {
                map[I] = MemberType::GetShortcut();
            }
            return map;
        }
        return std::unordered_map<size_t, std::string>{};
    }
    static inline auto IndexToShortcut = CreateIndexToShortcut();

    size_t GetId() const { return this->index(); }

    const std::string &GetName() const {
        return Call([](auto &a) -> const std::string & { return a.GetName(); });
    }
    const std::string &GetMenuLabel() const {
        return Call([](auto &a) -> const std::string & { return a.GetMenuLabel(); });
    }
    std::string GetShortcut() const {
        const auto id = GetId();
        return IndexToShortcut.contains(id) ? IndexToShortcut.at(id) : "";
    }

    bool IsAllowed() const {
        return Call([](auto &a) { return a.Allowed(); });
    }
    bool IsSavable() const {
        return Call([](auto &a) { return a._Meta.Savable; });
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
        if (GetId() != other.GetId()) return false;
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
    using type = Combine<ActionVariant<Ts1..., Ts2...>, Vars...>::type;
};

// Utility to filter an `ActionVariant` by a predicate.
// E.g. `using StatefulAction = Actionable::Filter<Actionable::IsSavable, Any>::type;`
template<template<typename> class Predicate, typename Var> struct Filter;
template<template<typename> class Predicate, typename... Types> struct Filter<Predicate, Actionable::ActionVariant<Types...>> {
    template<typename Type>
    using ConditionalAdd = std::conditional_t<Predicate<Type>::value, Actionable::ActionVariant<Type>, Actionable::ActionVariant<>>;
    using type = Actionable::Combine<ConditionalAdd<Types>...>::type;
};
} // namespace Actionable
