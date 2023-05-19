#pragma once

#include <concepts>
#include <variant>

// next up:
// - improve action IDs
//   - actions all get a path in addition to their name (start with all at root, but will be heirarchical soon)
//   - `ID` type generated from path, like `StateMember`:
//     `Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0))`
// - Add `Help` string (also like StateMember)
// Move all action declaration & `Apply` handling to domain files.

#include "../Helper/String.h"

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
namespace Action {
#define Define(ActionName, ...)                                                           \
    struct ActionName {                                                                   \
        inline static const string Name{StringHelper::PascalToSentenceCase(#ActionName)}; \
        inline static bool Allowed() { return true; }                                     \
        __VA_ARGS__;                                                                      \
    };

// Override `Allowed()` to return `false` if the action is not allowed in the current state.
#define DefineContextual(ActionName, ...)                                                 \
    struct ActionName {                                                                   \
        inline static const string Name{StringHelper::PascalToSentenceCase(#ActionName)}; \
        static bool Allowed();                                                            \
        __VA_ARGS__;                                                                      \
    };

template<typename T>
concept Actionable = requires() {
    { T::Name } -> std::same_as<const string &>;
};

// E.g. `Action::GetName<MyAction>()`
template<Actionable T> string GetName() { return T::Name; }

// Helper struct to initialize maps of `Actionable` names to their variant indices.
template<typename VariantType, size_t I = 0> struct CreateNameToIndexMap {
    using T = std::variant_alternative_t<I, VariantType>;
    static_assert(Actionable<T>, "`NameToIndexMap` must be called with a variant holding `Actionable` types.");

    static void Init(std::unordered_map<string, size_t> &name_to_index) {
        name_to_index[T::Name] = I;
        if constexpr (I + 1 < std::variant_size_v<VariantType>) {
            CreateNameToIndexMap<VariantType, I + 1>::Init(name_to_index);
        }
    }
};
} // namespace Action
