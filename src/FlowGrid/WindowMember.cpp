#include "WindowMember.h"

#include "imgui_internal.h"

#include "Store/StoreFwd.h"

void UIStateMember::DrawWindows() const {
    for (const auto *child : Children) {
        if (const auto *window_child = dynamic_cast<const Window *>(child)) {
            window_child->Draw();
        }
    }
    for (const auto *child : Children) {
        if (const auto *ui_child = dynamic_cast<const UIStateMember *>(child)) {
            ui_child->DrawWindows();
        }
    }
}

Window::Window(StateMember *parent, string_view path_segment, string_view name_help, const bool visible)
    : UIStateMember(parent, path_segment, name_help) {
    store::Set(Visible, visible, InitStore);
}
Window::Window(StateMember *parent, string_view path_segment, string_view name_help, const ImGuiWindowFlags flags)
    : UIStateMember(parent, path_segment, name_help), WindowFlags(flags) {}
Window::Window(StateMember *parent, string_view path_segment, string_view name_help, Menu menu)
    : UIStateMember(parent, path_segment, name_help), WindowMenu{std::move(menu)} {}

using namespace ImGui;

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

    if (Visible && !open) q(Action::SetValue{Visible.Path, false});
}

void Window::Dock(ID node_id) const {
    DockBuilderDockWindow(ImGuiLabel.c_str(), node_id);
}

void Window::MenuItem() const {
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, Visible)) q(Action::ToggleValue{Visible.Path});
}

void Window::SelectTab() const {
    FindImGuiWindow().DockNode->SelectedTabId = FindImGuiWindow().TabId;
}

void TabsWindow::Render(const std::set<ID> &exclude) const {
    if (BeginTabBar("")) {
        for (const auto *child : Children) {
            if (const auto *ui_child = dynamic_cast<const UIStateMember *>(child)) {
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
            Match(
                item,
                [](const Menu &menu) {
                    menu.Draw();
                },
                [](const MenuItemDrawable &drawable) {
                    drawable.MenuItem();
                },
                [](const Action::Any &action) {
                    const string menu_label = Action::GetMenuLabel(action);
                    const string shortcut = Action::GetShortcut(action);
                    if (ImGui::MenuItem(menu_label.c_str(), shortcut.c_str(), false, ActionAllowed(action))) {
                        Match(action, [](const auto &a) { q(a); });
                    }
                },
            );
        }
        if (IsMain) EndMainMenuBar();
        else if (is_menu_bar) EndMenuBar();
        else EndMenu();
    }
}
