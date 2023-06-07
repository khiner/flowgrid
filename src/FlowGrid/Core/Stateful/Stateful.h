#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Primitive.h"
#include "Helper/Path.h"
#include "UI/Drawable.h"

using std::string, std::string_view;

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
std::pair<string_view, string_view> ParseHelpText(string_view str);

namespace Stateful {
struct Base {
    inline static std::unordered_map<ID, Base *> WithId; // Access any state member by its ID.

    Base(Base *parent = nullptr, string_view path_segment = "", string_view name_help = "");
    Base(Base *parent, string_view path_segment, std::pair<string_view, string_view> name_help);

    virtual ~Base();
    const Base *Child(Count i) const { return Children[i]; }
    inline Count ChildCount() const { return Children.size(); }

    const Base *Parent;
    std::vector<Base *> Children{};
    const string PathSegment;
    const StorePath Path;
    const string Name, Help, ImGuiLabel;
    const ID Id;

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered. Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};
} // namespace Stateful

/**
Convenience macros for compactly defining `Stateful` types and their properties.

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
      1) `PropType`: Any type deriving from `Stateful`.
      2) `PropName` (use PascalCase) is used for:
        - The ID of the property, relative to its parent (`this` during the macro's execution).
        - The name of the instance variable added to `this` (again, defined like any other instance variable in a `Stateful`).
        - The default label displayed in the UI is a 'Sentense cased' label derived from the prop's 'PascalCase' `PropName` property-id/path-segment (the second arg).
      3) `NameHelp`
        - A string with format "Label string?Help string".
        - Optional, available with a `_` suffix.
        - Overrides the label displayed in the UI for this property.
        - Anything after a '?' is interpretted as a help string
          - E.g. `Prop(Bool, TestAThing, "Test-a-thing?A state member for testing things")` overrides the default "Test a thing" label with a hyphenation.
          - Or, provide nothing before the '?' to add a help string without overriding the default `PropName`-derived label.
            - E.g. "?A state member for testing things."
* Stateful types
  - `DefineStateful` defines a plain old state type.
  - `DefineUI` defines a drawable state type.
    - `DefineUI_` is the same as `DefineUI`, but adds a custom constructor implementation (with the same arguments).
  - `DefineWindow` defines a drawable state type whose contents are rendered to a window.
    - `DefineWindow_` is the same as `DefineWindow`, but allows passing either:
      - a `bool` to override the default `true` visibility, or
      - a `Menu` to define the window's menu.
    - `TabsWindow` is a `DefineWindow` that renders all its props as tabs. (except the `Visible` boolean member coming from `DefineWindow`).
  - todo Refactor docking behavior out of `DefineWindow` into a new `DefineDocked` type.

**/

#define Prop(PropType, PropName, ...) PropType PropName{this, (#PropName), "", __VA_ARGS__};
#define Prop_(PropType, PropName, NameHelp, ...) PropType PropName{this, (#PropName), (NameHelp), __VA_ARGS__};

#define DefineStateful(TypeName, ...)  \
    struct TypeName : Stateful::Base { \
        using Stateful::Base::Base;    \
        __VA_ARGS__;                   \
    };

struct UIStateful : Stateful::Base, Drawable {
    using Stateful::Base::Base;
    void DrawWindows() const; // Recursively draw all windows in the state tree. Note that non-window members can contain windows.
};

#define DefineUI(TypeName, ...)       \
    struct TypeName : UIStateful {    \
        using UIStateful::UIStateful; \
        __VA_ARGS__;                  \
                                      \
    protected:                        \
        void Render() const override; \
    };

#define DefineUI_(TypeName, ...)                                                                \
    struct TypeName : UIStateful {                                                              \
        TypeName(Stateful::Base *parent, string_view path_segment, string_view name_help = ""); \
        __VA_ARGS__;                                                                            \
                                                                                                \
    protected:                                                                                  \
        void Render() const override;                                                           \
    };
