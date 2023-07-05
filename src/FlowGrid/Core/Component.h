#pragma once

#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core/Primitive/Primitive.h"
#include "Helper/Path.h"
#include "Helper/Time.h"
#include "UI/Drawable.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

using std::string, std::string_view;

struct Store;

struct Menu : Drawable {
    using Item = std::variant<Menu, std::reference_wrapper<MenuItemDrawable>, std::function<void()>>;

    Menu(string_view label, const std::vector<const Item> items);
    explicit Menu(const std::vector<const Item> items);
    Menu(const std::vector<const Item> items, const bool is_main);

    const string Label; // If no label is provided, this is rendered as a top-level window menu bar.
    const std::vector<const Item> Items;
    const bool IsMain{false};

protected:
    void Render() const override;
};

struct ImGuiWindow;
using ImGuiWindowFlags = int;

// Copy of some of ImGui's flags, to avoid including `imgui.h` in this header.
// Be sure to keep these in sync, because they are used directly as values for their ImGui counterparts.
enum WindowFlags_ {
    WindowFlags_None = 0,
    WindowFlags_NoScrollbar = 1 << 3,
    WindowFlags_MenuBar = 1 << 10,
};

struct Component : Drawable {
    struct Metadata {
        // Split the string on '?'.
        // If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
        // todo don't split on escaped '\?'
        static Metadata Parse(string_view meta_str);
        const string Name, Help;
    };

    struct ComponentArgs {
        Component *Parent; // Must be present for non-empty constructor.
        string_view PathSegment = "";
        string_view MetaStr = "";
    };

    inline static std::unordered_map<ID, Component *> ById; // Access any state member by its ID.
    // Components with at least one descendent field updated during the latest action pass.
    inline static std::unordered_set<ID> ChangedComponentIds;

    Component(Store &);
    Component(ComponentArgs &&);
    Component(ComponentArgs &&, ImGuiWindowFlags flags);
    Component(ComponentArgs &&, Menu &&menu);

    virtual ~Component();

    Component(const Component &) = delete; // Copying not allowed.
    Component &operator=(const Component &) = delete; // Assignment not allowed.

    virtual void RenderDebug() const {}

    // Render a nested tree of components, with Fields as leaf nodes displaying their values as text.
    // By default, renders `this` a node with children as child nodes.
    virtual void RenderValueTree(bool annotate, bool auto_select) const;

    const Component *Child(Count i) const noexcept { return Children[i]; }
    inline Count ChildCount() const noexcept { return Children.size(); }

    Store &RootStore; // Reference to the store at the root of this component's tree.
    const Component *Parent; // Only null for the root component.
    std::vector<Component *> Children{};
    const string PathSegment;
    const StorePath Path;
    const string Name, Help, ImGuiLabel;
    const ID Id;

    // todo this should be separated after current window refactor.
    ImGuiWindow &FindImGuiWindow() const;
    void Dock(ID node_id) const;
    void SelectTab() const; // If this window is tabbed, select it.
    const Menu WindowMenu{{}};
    const ImGuiWindowFlags WindowFlags{WindowFlags_None};

    // Child renderers.
    void RenderTabs() const;
    void RenderTreeNodes() const;

protected:
    virtual void Render() const override {} // By default, components don't render anything.

    // Helper to display a (?) mark which shows a tooltip when hovered. Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;

    static bool TreeNode(std::string_view label, bool highlight_label = false, const char *value = nullptr, bool highlight_value = false);

private:
    Component(Component *parent, string_view path_segment, Metadata meta, ImGuiWindowFlags flags, Menu &&menu);
};

// Minimal/base debug component.
// Actual debug content is rendered in the parent component's `RenderDebug()`,
// and debug components themselves can't further override `RenderDebug()`.
// Otherwise, debug components are just like regular components - they store additional config fields, be rendered as windows, etc.
// Override `Render` to render anything other than just the parent's debug content.
struct DebugComponent : Component {
    using Component::Component;

protected:
    virtual void Render() const override { RenderDebug(); }

private:
    void RenderDebug() const override { Parent->RenderDebug(); } // Not overridable.
};

/**
Convenience macros for compactly defining `Component` types and their properties.

todo These will very likely be defined in a separate language once the API settles down.
  If we could hot-reload and only recompile the needed bits without restarting the app, it would accelerate development A TON.
  (Long compile times, although they aren't nearly as bad as [my previous attempt](https://github.com/khiner/flowgrid_old),
  are still the biggest drain on this project.)

Macros:

All macros end in semicolons already, so there's no strict need to suffix their usage with a semicolon.
However, all macro calls in FlowGrid are treated like regular function calls, appending a semicolon.
todo If we stick with this, add a `static_assert(true, "")` to the end of all macro definitions.
https://stackoverflow.com/questions/35530850/how-to-require-a-semicolon-after-a-macro
todo Try out replacing semicolon separators by e.g. commas.

* Properties
  - `Prop` adds a new property `this`.
  Define a property of this type during construction at class scope to add a child member variable.
    - Assumes it's being called within a `PropType` class scope during construction.
    `PropType`, with variable name `PropName`, constructing the state member with `this` as a parent, and store path-segment `"{PropName}"`.
    (string with value the same as the variable name).
  - `Prop_` is the same as `Prop`, but supports overriding the displayed name & adding help text in the third arg.
  - Arguments
    1) `PropType`: Any type deriving from `Component`.
    2) `PropName` (use PascalCase) is used for:
      - The ID of the property, relative to its parent (`this` during the macro's execution).
      - The name of the instance variable added to `this` (again, defined like any other instance variable in a `Component`).
      - The default label displayed in the UI is a 'Sentense cased' label derived from the prop's 'PascalCase' `PropName` property-id/path-segment (the second arg).
    3) `MetaStr`
      - Metadata string with format "Label string?Help string".
      - Optional, available with a `_` suffix.
      - Overrides the label displayed in the UI for this property.
      - Anything after a '?' is interpretted as a help string
        - E.g. `Prop(Bool, TestAThing, "Test-a-thing?A state member for testing things")` overrides the default "Test a thing" label with a hyphenation.
        - Or, provide nothing before the '?' to add a help string without overriding the default `PropName`-derived label.
          - E.g. "?A state member for testing things."
**/

#define Prop(PropType, PropName, ...) PropType PropName{{this, #PropName, ""}, __VA_ARGS__};
#define Prop_(PropType, PropName, MetaStr, ...) PropType PropName{{this, #PropName, MetaStr}, __VA_ARGS__};
