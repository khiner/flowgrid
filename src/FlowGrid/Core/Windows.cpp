#include "Windows.h"

#include "imgui_internal.h"

void Windows::SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &components) {
    WindowComponentIds.clear();
    VisibleComponents.Clear();
    for (const auto &component : components) {
        WindowComponentIds.insert(component.get().Id);
        VisibleComponents.Insert(component.get().Id);
    }
}

bool Windows::IsWindow(ID component_id) const { return WindowComponentIds.contains(component_id); }
bool Windows::IsVisible(ID component_id) const { return VisibleComponents.Contains(component_id); }

void Windows::ToggleVisible(ID component_id) const {
    if (!VisibleComponents.Contains(component_id)) VisibleComponents.Insert(component_id);
    else VisibleComponents.Erase(component_id);
}

using namespace ImGui;

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

void Windows::ToggleDebugMenuItem(const Component &component) const {
    if (MenuItem(component.ImGuiLabel.c_str(), nullptr, IsVisible(component.Id))) {
        Q(Action::Windows::ToggleDebug{component.Id});
    }
}
