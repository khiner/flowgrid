#include "Window.h"

#include "imgui_internal.h"

void UIComponent::DrawWindows() const {
    for (const auto *child : Children) {
        if (const auto *window_child = dynamic_cast<const Window *>(child)) {
            window_child->Draw();
        }
    }
    for (const auto *child : Children) {
        if (const auto *ui_child = dynamic_cast<const UIComponent *>(child)) {
            ui_child->DrawWindows();
        }
    }
}

using namespace ImGui;

Window::Window(Component *parent, string_view path_leaf, string_view meta_str, const bool visible)
    : UIComponent(parent, path_leaf, meta_str) {
    store::Set(Visible, visible);
}
Window::Window(Component *parent, string_view path_leaf, string_view meta_str, const ImGuiWindowFlags flags)
    : UIComponent(parent, path_leaf, meta_str), WindowFlags(flags) {}
Window::Window(Component *parent, string_view path_leaf, string_view meta_str, Menu menu)
    : UIComponent(parent, path_leaf, meta_str), WindowMenu{std::move(menu)} {}

ImGuiWindow &Window::FindImGuiWindow() const { return *FindWindowByName(ImGuiLabel.c_str()); }

void Window::Draw() const {
    if (!Visible) return;

    ImGuiWindowFlags flags = WindowFlags;
    if (!WindowMenu.Items.empty()) flags |= ImGuiWindowFlags_MenuBar;

    bool open = Visible;
    if (Begin(ImGuiLabel.c_str(), &open, flags) && open) {
        WindowMenu.Draw();
        Render();
    }
    End();

    if (Visible && !open) Action::Primitive::Set{Visible.Path, false}.q();
}

void Window::Dock(ID node_id) const {
    DockBuilderDockWindow(ImGuiLabel.c_str(), node_id);
}

void Window::MenuItem() const {
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, Visible)) Action::Primitive::ToggleBool{Visible.Path}.q();
}

void Window::SelectTab() const {
    FindImGuiWindow().DockNode->SelectedTabId = FindImGuiWindow().TabId;
}

void TabsWindow::Render(const std::set<ID> &exclude) const {
    if (BeginTabBar("")) {
        for (const auto *child : Children) {
            if (const auto *ui_child = dynamic_cast<const UIComponent *>(child)) {
                if (!exclude.contains(ui_child->Id) && ui_child->Id != Visible.Id && BeginTabItem(child->ImGuiLabel.c_str())) {
                    ui_child->Draw();
                    EndTabItem();
                }
            }
        }
        EndTabBar();
    }
}

void TabsWindow::Render() const {
    static const std::set<ID> exclude = {};
    TabsWindow::Render(exclude);
}

Menu::Menu(string_view label, const std::vector<const Item> items) : Label(label), Items(std::move(items)) {}
Menu::Menu(const std::vector<const Item> items) : Menu("", std::move(items)) {}
Menu::Menu(const std::vector<const Item> items, const bool is_main) : Label(""), Items(std::move(items)), IsMain(is_main) {}

void Menu::Render() const {
    if (Items.empty()) return;

    const bool is_menu_bar = Label.empty();
    if (IsMain ? BeginMainMenuBar() : (is_menu_bar ? BeginMenuBar() : BeginMenu(Label.c_str()))) {
        for (const auto &item : Items) {
            Visit(
                item,
                [](const Menu &menu) {
                    menu.Draw();
                },
                [](const MenuItemDrawable &drawable) {
                    drawable.MenuItem();
                },
                [](const std::function<void()> &draw) {
                    draw();
                }
            );
        }
        if (IsMain) EndMainMenuBar();
        else if (is_menu_bar) EndMenuBar();
        else EndMenu();
    }
}
