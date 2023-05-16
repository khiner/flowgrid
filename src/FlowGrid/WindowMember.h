#pragma once

#include <set>

#include "Action/Action.h" // Windows have menus, which can perform actions.
#include "Field.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

struct Menu : Drawable {
    using Item = std::variant<
        const Menu,
        const std::reference_wrapper<MenuItemDrawable>,
        const EmptyAction>;

    Menu(string_view label, const vector<const Item> items);
    explicit Menu(const vector<const Item> items);
    Menu(const vector<const Item> items, const bool is_main);

    const string Label; // If no label is provided, this is rendered as a top-level window menu bar.
    const vector<const Item> Items;
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

using namespace Field;

struct Window : UIStateMember, MenuItemDrawable {
    using UIStateMember::UIStateMember;

    Window(StateMember *parent, string_view path_segment, string_view name_help, bool visible);
    Window(StateMember *parent, string_view path_segment, string_view name_help, ImGuiWindowFlags flags);
    Window(StateMember *parent, string_view path_segment, string_view name_help, Menu menu);

    ImGuiWindow &FindImGuiWindow() const;
    void Draw() const override;
    void MenuItem() const override; // Rendering a window as a menu item shows a window visibility toggle, with the window name as the label.
    void Dock(ID node_id) const;
    void SelectTab() const; // If this window is tabbed, select it.

    Prop(Bool, Visible, true);

    const Menu WindowMenu{{}};
    const ImGuiWindowFlags WindowFlags{WindowFlags_None};
};

#define WindowMember(MemberName, ...) \
    struct MemberName : Window {      \
        using Window::Window;         \
        __VA_ARGS__;                  \
                                      \
    protected:                        \
        void Render() const override; \
    };

#define WindowMember_(MemberName, VisibleOrMenu, ...)                                         \
    struct MemberName : Window {                                                              \
        MemberName(StateMember *parent, string_view path_segment, string_view name_help = "") \
            : Window(parent, path_segment, name_help, (VisibleOrMenu)) {}                     \
        __VA_ARGS__;                                                                          \
                                                                                              \
    protected:                                                                                \
        void Render() const override;                                                         \
    };

// When we define a window member type without adding properties, we're defining a new way to arrange and draw the children of the window.
// The controct we're signing up for is to implement `void TabsWindow::Render() const`.
WindowMember(
    TabsWindow,

    protected
    : void Render(const std::set<ID> &exclude) const;
);