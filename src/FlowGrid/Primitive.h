#pragma once

#include <concepts>
#include <string>
#include <variant>
#include <vector>

using std::string;

/**
Redefining [ImGui's scalar data types](https://github.com/ocornut/imgui/blob/master/imgui.h#L223-L232)

This is done in order to:
  * clarify & document the actual meanings of the FlowGrid integer type aliases below, and
  * emphasize the importance of FlowGrid integer types reflecting ImGui types.

If it wasn't important to keep FlowGrid's integer types mapped 1:1 to ImGui's, we would be using
 [C++11's fixed width integer types](https://en.cppreference.com/w/cpp/types/integer) instead.

Make sure to double check once in a blue moon that the ImGui types have not changed!
*/
using ImS8 = signed char; // 8-bit signed integer
using ImU8 = unsigned char; // 8-bit unsigned integer
using ImS16 = signed short; // 16-bit signed integer
using ImU16 = unsigned short; // 16-bit unsigned integer
using ImS32 = signed int; // 32-bit signed integer == int
using ImU32 = unsigned int; // 32-bit unsigned integer (used to store packed colors & positions)
using ImS64 = signed long long; // 64-bit signed integer
using ImU64 = unsigned long long; // 64-bit unsigned integer

// Scalar data types, pointing to ImGui scalar types, with `{TypeName} = Im{TypeName}`.
using S8 = ImS8;
using U8 = ImU8;
using S16 = ImS16;
using U16 = ImU16;
using S32 = ImS32;
using U32 = ImU32;
using S64 = ImS64;
using U64 = ImU64;

using Count = U32;

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

static constexpr string to_string(uint value) noexcept { return std::to_string(int(value)); }
