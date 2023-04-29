#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

#include "Primitive.h"

using std::unordered_map, std::string_view, std::vector;

struct StateMember {
    inline static unordered_map<ID, StateMember *> WithId; // Access any state member by its ID.

    StateMember(StateMember *parent = nullptr, string_view path_segment = "", string_view name_help = "");
    StateMember(StateMember *parent, string_view path_segment, std::pair<string_view, string_view> name_help);

    virtual ~StateMember();
    const StateMember *Child(Count i) const { return Children[i]; }
    inline Count ChildCount() const { return Children.size(); }

    const StateMember *Parent;
    vector<StateMember *> Children{};
    const string PathSegment;
    const StatePath Path;
    const string Name, Help, ImGuiLabel;
    const ID Id;

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered. Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};
