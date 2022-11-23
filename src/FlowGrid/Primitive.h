#pragma once

#include <string>
#include <variant>

using std::string;

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
