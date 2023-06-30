#include "Component.h"

#include "imgui_internal.h"
#include <format>

#include "Helper/String.h"
#include "UI/HelpMarker.h"

Menu::Menu(string_view label, const std::vector<const Item> items) : Label(label), Items(std::move(items)) {}
Menu::Menu(const std::vector<const Item> items) : Menu("", std::move(items)) {}
Menu::Menu(const std::vector<const Item> items, const bool is_main) : Label(""), Items(std::move(items)), IsMain(is_main) {}

Component::Metadata Component::Metadata::Parse(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? string(str.substr(0, help_split)) : string(str), found ? string(str.substr(help_split + 1)) : ""};
}

Component::Component(Component *parent, string_view path_leaf, Metadata meta, ImGuiWindowFlags flags, Menu &&menu)
    : Parent(parent),
      PathLeaf(path_leaf),
      Path(Parent && !PathLeaf.empty() ? (Parent->Path / PathLeaf) : (Parent ? Parent->Path : (!PathLeaf.empty() ? StorePath(PathLeaf) : RootPath))),
      Name(meta.Name.empty() ? PathLeaf.empty() ? "" : StringHelper::PascalToSentenceCase(PathLeaf) : meta.Name),
      Help(meta.Help),
      ImGuiLabel(Name.empty() ? "" : std::format("{}##{}", Name, PathLeaf)),
      Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0)),
      WindowMenu(std::move(menu)),
      WindowFlags(flags) {
    if (parent) parent->Children.emplace_back(this);
    WithId[Id] = this;
}

Component::Component(ComponentArgs &&args)
    : Component(std::move(args.Parent), std::move(args.PathLeaf), Metadata::Parse(std::move(args.MetaStr)), ImGuiWindowFlags_None, Menu{{}}) {}

Component::Component(ComponentArgs &&args, const ImGuiWindowFlags flags)
    : Component(std::move(args.Parent), std::move(args.PathLeaf), Metadata::Parse(std::move(args.MetaStr)), flags, Menu{{}}) {}
Component::Component(ComponentArgs &&args, Menu &&menu)
    : Component(std::move(args.Parent), std::move(args.PathLeaf), Metadata::Parse(std::move(args.MetaStr)), ImGuiWindowFlags_None, std::move(menu)) {}

Component::~Component() {
    WithId.erase(Id);
}

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void Component::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) ImGui::SameLine();
    fg::HelpMarker(Help.c_str());
    if (!after) ImGui::SameLine();
}

// Currently, `Draw` is not used for anything except wrapping around `Render`,
// but it's here in case we want to do something like monitoring or ID management in the future.
void Drawable::Draw() const {
    // ImGui widgets all push the provided label to the ID stack,
    // but info hovering isn't complete yet, and something like this might be needed...
    // PushID(ImGuiLabel.c_str());
    Render();
    // PopID();
}

using namespace ImGui;

void Menu::Render() const {
    if (Items.empty()) return;

    const bool is_menu_bar = Label.empty();
    if (IsMain ? BeginMainMenuBar() : (is_menu_bar ? BeginMenuBar() : BeginMenu(Label.c_str()))) {
        for (const auto &item : Items) {
            Visit(
                item,
                [](const Menu &menu) { menu.Draw(); },
                [](const MenuItemDrawable &drawable) { drawable.MenuItem(); },
                [](const std::function<void()> &draw) { draw(); }
            );
        }
        if (IsMain) EndMainMenuBar();
        else if (is_menu_bar) EndMenuBar();
        else EndMenu();
    }
}

ImGuiWindow &Component::FindImGuiWindow() const { return *FindWindowByName(ImGuiLabel.c_str()); }

void Component::Dock(ID node_id) const {
    DockBuilderDockWindow(ImGuiLabel.c_str(), node_id);
}

void Component::SelectTab() const {
    FindImGuiWindow().DockNode->SelectedTabId = FindImGuiWindow().TabId;
}

void Component::RenderTabs() const {
    if (BeginTabBar("")) {
        for (const auto *child : Children) {
            if (BeginTabItem(child->ImGuiLabel.c_str())) {
                child->Draw();
                EndTabItem();
            }
        }
        EndTabBar();
    }
}

void Component::RenderTreeNodes() const {
    for (const auto *child : Children) {
        if (TreeNodeEx(child->ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            child->Draw();
            TreePop();
        }
    }
}
