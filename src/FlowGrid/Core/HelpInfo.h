#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "Core/Primitive/ID.h"

struct HelpInfo {
    const std::string Name, Help;

    // Split the string on '?'.
    // If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
    // todo don't split on escaped '\?'
    static HelpInfo Parse(std::string_view meta_str);

    // Metadata for display in the Info stack.
    inline static std::unordered_map<ID, HelpInfo> ById{};
};
