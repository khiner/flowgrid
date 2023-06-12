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

struct Window : UIComponent, MenuItemDrawable {
    using UIComponent::UIComponent;

    Window(ComponentArgs &&, bool visible);
    Window(ComponentArgs &&, ImGuiWindowFlags flags);
    Window(ComponentArgs &&, Menu &&menu);

    ImGuiWindow &FindImGuiWindow() const;
    void Draw() const override;
    void MenuItem() const override; // Rendering a window as a menu item shows a window visibility toggle, with the window name as the label.
    void Dock(ID node_id) const;
    void SelectTab() const; // If this window is tabbed, select it.

    Prop(Bool, Visible, true);

    const Menu WindowMenu{{}};
    const ImGuiWindowFlags WindowFlags{WindowFlags_None};
};

#define DefineWindow(TypeName, ...)   \
    struct TypeName : Window {        \
        using Window::Window;         \
        __VA_ARGS__;                  \
                                      \
    protected:                        \
        void Render() const override; \
    };

#define DefineWindow_(TypeName, VisibleOrMenu, ...)       \
    struct TypeName : Window {                            \
        TypeName(ComponentArgs &&args)                    \
            : Window(std::move(args), (VisibleOrMenu)) {} \
        __VA_ARGS__;                                      \
                                                          \
    protected:                                            \
        void Render() const override;                     \
    };

// When we define a window member type without adding properties, we're defining a new way to arrange and draw the children of the window.
struct TabsWindow : Window {
    using Window::Window;

protected:
    void Render() const override;
    void Render(const std::set<ID> &exclude) const;
};
