#pragma once

#include <string>
#include <variant>

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
