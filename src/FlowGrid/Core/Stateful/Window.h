#pragma once

#include <set>

#include "Core/Field/Bool.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

template<typename T>
concept CanDrawMenuItem = requires(const T t) {
    { t.MenuItem() } -> std::same_as<void>;
};

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

namespace Stateful {
struct Window : UIStateful, MenuItemDrawable {
    using UIStateful::UIStateful;

    Window(Stateful::Base *parent, string_view path_segment, string_view name_help, bool visible);
    Window(Stateful::Base *parent, string_view path_segment, string_view name_help, ImGuiWindowFlags flags);
    Window(Stateful::Base *parent, string_view path_segment, string_view name_help, Menu menu);

    ImGuiWindow &FindImGuiWindow() const;
    void Draw() const override;
    void MenuItem() const override; // Rendering a window as a menu item shows a window visibility toggle, with the window name as the label.
    void Dock(ID node_id) const;
    void SelectTab() const; // If this window is tabbed, select it.

    Prop(Bool, Visible, true);

    const Menu WindowMenu{{}};
    const ImGuiWindowFlags WindowFlags{WindowFlags_None};
};
} // namespace Stateful

#define DefineWindow(TypeName, ...)      \
    struct TypeName : Stateful::Window { \
        using Stateful::Window::Window;  \
        __VA_ARGS__;                     \
                                         \
    protected:                           \
        void Render() const override;    \
    };

#define DefineWindow_(TypeName, VisibleOrMenu, ...)                                            \
    struct TypeName : Stateful::Window {                                                       \
        TypeName(Stateful::Base *parent, string_view path_segment, string_view name_help = "") \
            : Stateful::Window(parent, path_segment, name_help, (VisibleOrMenu)) {}            \
        __VA_ARGS__;                                                                           \
                                                                                               \
    protected:                                                                                 \
        void Render() const override;                                                          \
    };

// When we define a window member type without adding properties, we're defining a new way to arrange and draw the children of the window.
// The controct we're signing up for is to implement `void TabsWindow::Render() const`.
DefineWindow(
    TabsWindow,

    protected
    : void Render(const std::set<ID> &exclude) const;
);
