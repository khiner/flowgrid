#include "Windows.h"

#include "Core/Store/Store.h"

#include "imgui.h"

void Windows::SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &components) {
    WindowComponentIds.reserve(components.size());
    for (const auto &component : components) WindowComponentIds.push_back(component.get().Id);
    std::vector<bool> visible_components(components.size(), true);
    VisibleComponents.Set(visible_components);
}

void Windows::Apply(const ActionType &action) const {
    Visit(
        action,
        [&](const Action::Windows::ToggleVisible &a) {
            const auto index = std::find(WindowComponentIds.begin(), WindowComponentIds.end(), a.component_id) - WindowComponentIds.begin();
            VisibleComponents.Set(index, !VisibleComponents[index]);
        }
    )
}

using namespace ImGui;

void Windows::Render() const {
    for (size_t i = 0; i < WindowComponentIds.size(); i++) {
        if (!VisibleComponents[i]) continue;

        const auto component_id = WindowComponentIds[i];
        const auto *component = Component::WithId[component_id];
        if (auto *drawable_component = dynamic_cast<const Drawable *>(component)) {
            ImGuiWindowFlags flags = component->WindowFlags;
            if (!component->WindowMenu.Items.empty()) flags |= ImGuiWindowFlags_MenuBar;

            bool open = true;
            if (Begin(component->ImGuiLabel.c_str(), &open, flags)) {
                component->WindowMenu.Draw();
                drawable_component->Draw();
            }
            End();

            if (!open) Action::Windows::ToggleVisible{component->Id}.q();
        }
    }
}

void Windows::MenuItem() const {
    // if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, Visible)) Action::Windows::ToggleVisible{Path}.q();

    // Menu(
    //     "Windows",
    //     {
    //         Menu(
    //             "Faust",
    //             {
    //                 Menu("Editor", {Audio.Faust.Code, Audio.Faust.Code.Metrics}),
    //                 Audio.Faust.Graph,
    //                 Audio.Faust.Params,
    //                 Audio.Faust.Log,
    //             }
    //         ),
    //         Audio,
    //         Style,
    //         Demo,
    //         Menu(
    //             "Debug",
    //             {Debug.Metrics, Debug.DebugLog, Debug.StackTool, Debug.StateViewer, Debug.StorePathUpdateFrequency, Debug.ProjectPreview}
    //             // Debug.StateMemoryEditor,
    //         ),
    //     }
    // )
}
