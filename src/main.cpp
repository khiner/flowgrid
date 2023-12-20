#include "imgui.h"

#include "FlowGrid/Core/Store/Store.h"
#include "FlowGrid/Project/Project.h"
#include "UI/Fonts.h"
#include "UI/UIContext.h"

Store store{};
ActionQueue<Action::Any> action_queue{};
Project MainProject{store, action_queue};
// `project` is the only remaining global variable.
// I "just" need to finish refactoring action queueing to get rid of this.
const Project &project = MainProject;

bool Tick(const UIContext &ui) {
    static auto &io = ImGui::GetIO();

    bool running = ui.Tick(project);
    if (running && io.WantSaveIniSettings) {
        // ImGui sometimes sets this flags when settings have not actually changed.
        // E.g. if you press and hold a window-resize bar, it will set this flag every frame,
        // even if the cursor remains stationary (no window size change).
        // Rather than modifying the ImGui fork to not set this flag in all such cases
        // (which would likely be a rabbit hole), we just check for diffs here.
        ImGui::SaveIniSettingsToMemory(); // Populate the `Settings` context members.
        const auto &patch = MainProject.ImGuiSettings.CreatePatch(ImGui::GetCurrentContext());
        if (!patch.Empty()) MainProject.Q(Action::Store::ApplyPatch{patch});
        io.WantSaveIniSettings = false;
    }

    return running;
}

int main() {
    const UIContext ui{MainProject.ImGuiSettings, MainProject.Style}; // Initialize ImGui and other UI state.
    Component::gFonts.Init(); // Must be done after initializing ImGui.
    ImGui::GetIO().FontGlobalScale = ui.Style.ImGui.FontScale / Fonts::AtlasScale;

    // Initialize the global canonical store with all project state values set during project initialization.
    store.Commit();

    // Ensure all store values set during initialization are reflected in cached field/collection values, and all side effects are run.
    Component::RefreshAll();

    IGFD::Init();

    {
        // Relying on these rendering side effects up front is not great.
        Tick(ui); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the project state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Tick(ui); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        MainProject.ApplyQueuedActions(true);
    }

    MainProject.OnApplicationLaunch();

    while (Tick(ui)) {
        // Disable all actions while the file dialog is open.
        MainProject.ApplyQueuedActions(false, MainProject.FileDialog.Visible);
    }

    IGFD::Uninit();

    return 0;
}
