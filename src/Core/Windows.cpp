#include "Windows.h"

#include "imgui_internal.h"

void Windows::RegisterWindow(ID id) {
    WindowComponentIds.insert(id);
    VisibleComponents.Insert(id);
}
bool Windows::IsWindow(ID id) const { return WindowComponentIds.contains(id); }
bool Windows::IsVisible(ID id) const { return VisibleComponents.Contains(id); }
void Windows::ToggleVisible(ID id) const {
    if (!VisibleComponents.Contains(id)) VisibleComponents.Insert(id);
    else VisibleComponents.Erase(id);
}

using namespace ImGui;

void Windows::DrawMenuItem(const Component &c) const {
    if (MenuItem(c.ImGuiLabel.c_str(), nullptr, IsVisible(c.Id))) {
        Q(Action::Windows::ToggleVisible{c.Id});
    }
};

void Windows::Render() const {
    for (const ID id : VisibleComponents.Get()) {
        const auto *component = Component::ById.at(id);
        ImGuiWindowFlags flags = component->WindowFlags;
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
