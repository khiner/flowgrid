#pragma once

#include <string_view>

struct Component;
struct ComponentArgs {
    Component *Parent{nullptr};
    std::string_view PathSegment{""};
    std::string_view MetaStr{""};
    std::string_view PathSegmentPrefix{""};
};
