#include "Windows.h"

#include "imgui_internal.h"

bool Windows::IsDock(ID id) const { return DockComponentIds.contains(id); }

void Windows::Register(TransientStore &s, ID id, bool dock) {
    WindowComponentIds.insert(id);
    VisibleComponentIds.Insert(s, id);
    if (dock) DockComponentIds.insert(id);
}
bool Windows::IsWindow(ID id) const { return WindowComponentIds.contains(id); }
bool Windows::IsVisible(ID id) const { return VisibleComponentIds.Get().count(id); }
void Windows::ToggleVisible(TransientStore &s, ID id) const {
    if (!VisibleComponentIds.Get().count(id)) VisibleComponentIds.Insert(s, id);
    else VisibleComponentIds.Erase(s, id);
}

using namespace ImGui;

void Windows::DrawMenuItem(const Component &c) const {
    if (MenuItem(c.ImGuiLabel.c_str(), nullptr, IsVisible(c.Id))) {
        Q(Action::Windows::ToggleVisible{c.Id});
    }
};

void Windows::Render() const {
    for (const ID id : VisibleComponentIds.Get()) {
        const auto *component = Component::ById.at(id);
        auto flags = component->WindowFlags;
        if (!component->WindowMenu.Items.empty()) flags |= ImGuiWindowFlags_MenuBar;

        bool open = true;
        if (Begin(component->ImGuiLabel.c_str(), &open, flags)) {
            component->WindowMenu.Draw();
            component->Draw();
        }
        End();

        if (!open) Q(Action::Windows::ToggleVisible{id});
    }
}
