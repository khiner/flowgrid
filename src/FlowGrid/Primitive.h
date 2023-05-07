#pragma once

#include <concepts>
#include <string>
#include <variant>
#include <vector>

#include "Scalar.h"

using std::string;

using Primitive = std::variant<bool, U32, S32, float, string>;

// `IsVariantMember` is based on https://stackoverflow.com/a/45892305/780425.
template<typename T, typename VARIANT_T>
struct IsVariantMember;

template<typename T, typename... ALL_T>
struct IsVariantMember<T, std::variant<ALL_T...>>
    : public std::disjunction<std::is_same<T, ALL_T>...> {};

template<typename T>
concept IsPrimitive = IsVariantMember<T, Primitive>::value;

/**
An ID is used to uniquely identify something.

**Notable usage:**
`StateMember::Id` reflects the state member's `StatePath Path`, using `ImHashStr` to calculate its own `Id` using its parent's `Id` as a seed.
In the same way, each segment in `StateMember::Path` is calculated by appending its own `PathSegment` to its parent's `Path`.
This exactly reflects the way ImGui calculates its window/tab/dockspace/etc. ID calculation.
A drawable `UIStateMember` uses its `ID` (which is also an `ImGuiID`) as the ID for the top-level `ImGui` widget rendered during its `Draw` call.
This results in the nice property that we can find any `UIStateMember` instance by calling `StateMember::WithId.contains(ImGui::GetHoveredID())` any time during a `UIStateMember::Draw`.
 */
using ID = unsigned int;

#include <__filesystem/path.h>

namespace fs = std::filesystem;

using StatePath = fs::path;
using StoreEntry = std::pair<StatePath, Primitive>;
using StoreEntries = std::vector<StoreEntry>;

struct StatePathHash {
    auto operator()(const StatePath &p) const noexcept { return fs::hash_value(p); }
};

inline static const StatePath RootPath{"/"};

string to_string(const Primitive &); // xxx this is implemented in `StateJson.cpp`
