#pragma once

#include <string_view>

/**
An ID is used to uniquely identify something.

**Notable usage:**
`Component::Id` reflects the state member's `StorePath Path`, using `ImHashStr` to calculate its own `Id` using its parent's `Id` as a seed.
In the same way, each segment in `Component::Path` is calculated by appending its own `PathSegment` to its parent's `Path`.
This exactly reflects the way ImGui calculates its window/tab/dockspace/etc. ID calculation.
A drawable `Component` uses its `ID` (which is also an `ImGuiID`) as the ID for the top-level `ImGui` widget rendered during its `Draw` call.
This results in the nice property that we can find any `Component` instance by calling `Component::ById.contains(ImGui::GetHoveredID())` any time during a `Draw`.
 */
using ID = unsigned int; // Same type as `ImGuiID`

ID GenerateId(ID parent_id, ID child_id);
ID GenerateId(ID parent_id, std::string_view child_id);
