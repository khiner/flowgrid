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
    else VisibleComponents.Erase_(component_id);
}

void Windows::Apply(const ActionType &action) const {
    std::visit(
        Match{
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
        },
        action
    );
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

void Windows::ToggleMenuItem(const Component &component) const {
    if (MenuItem(component.ImGuiLabel.c_str(), nullptr, IsVisible(component.Id))) {
        Q(Action::Windows::ToggleVisible{component.Id});
    }
}

void Windows::ToggleDebugMenuItem(const Component &component) const {
    if (MenuItem(component.ImGuiLabel.c_str(), nullptr, IsVisible(component.Id))) {
        Q(Action::Windows::ToggleDebug{component.Id});
    }
}
