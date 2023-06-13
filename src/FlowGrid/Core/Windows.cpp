#include "Windows.h"

#include "Core/Store/Store.h"

#include "imgui.h"

void Windows::SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &components) const {
    std::vector<ID> ids;
    ids.reserve(components.size());
    for (const auto &component : components) ids.push_back(component.get().Id);
    VisibleComponents.Set(ids);
}

void Windows::Apply(const Action::Windows::Any &action) const {
    Visit(
        action,
        [&](const Action::Windows::ToggleVisible &a) {
            if (VisibleComponents.Contains(a.component_id)) {
                VisibleComponents.Erase(a.component_id);
            } else {
                VisibleComponents.Append(a.component_id);
            }
        }
    )
}

using namespace ImGui;

void Windows::Render() const {
    for (ID id : VisibleComponents) {
        const auto *component = Component::WithId[id];
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
