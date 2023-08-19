#include "Windows.h"

#include "imgui_internal.h"

void Windows::SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &components) {
    WindowComponentIds.clear();
    WindowComponentIds.reserve(components.size());
    for (const auto &component : components) WindowComponentIds.push_back(component.get().Id);
    VisibleComponents.Set(WindowComponentIds);
}

bool Windows::IsWindow(ID component_id) const { return std::ranges::find(WindowComponentIds, component_id) != WindowComponentIds.end(); }
bool Windows::IsVisible(ID component_id) const { return VisibleComponents.Contains(component_id); }

void Windows::ToggleVisible(ID component_id) const {
    if (!VisibleComponents.Contains(component_id)) VisibleComponents.PushBack(component_id);
    else VisibleComponents.Erase(component_id);
}

void Windows::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Windows::ToggleVisible &a) { ToggleVisible(a.component_id); },
        [this](const Action::Windows::ToggleDebug &a) {
            const bool toggling_on = !VisibleComponents.Contains(a.component_id);
            ToggleVisible(a.component_id);
            if (!toggling_on) return;

            auto *debug_component = static_cast<DebugComponent *>(Component::ById.at(a.component_id));
            if (auto *window = debug_component->FindDockWindow()) {
                auto docknode_id = window->DockId;
                auto debug_node_id = ImGui::DockBuilderSplitNode(docknode_id, ImGuiDir_Right, debug_component->SplitRatio, nullptr, &docknode_id);
                debug_component->Dock(debug_node_id);
            }
        },
    )
}

using namespace ImGui;

void Windows::Render() const {
    for (const ID component_id : VisibleComponents) {
        const auto *component = Component::ById.at(component_id);
        ImGuiWindowFlags flags = component->WindowFlags;
        if (!component->WindowMenu.Items.empty()) flags |= ImGuiWindowFlags_MenuBar;

        bool open = true;
        if (Begin(component->ImGuiLabel.c_str(), &open, flags)) {
            component->WindowMenu.Draw();
            component->Draw();
        }
        End();

        if (!open) Action::Windows::ToggleVisible{component_id}.q();
    }
}

void Windows::ToggleMenuItem(const Component &component) const {
    if (MenuItem(component.ImGuiLabel.c_str(), nullptr, IsVisible(component.Id))) {
        Action::Windows::ToggleVisible{component.Id}.q();
    }
}

void Windows::ToggleDebugMenuItem(const Component &component) const {
    if (MenuItem(component.ImGuiLabel.c_str(), nullptr, IsVisible(component.Id))) {
        Action::Windows::ToggleDebug{component.Id}.q();
    }
}
