#include "imgui.h"

#include "FlowGrid/Core/Store/Store.h"
#include "FlowGrid/Core/Store/StoreHistory.h"
#include "FlowGrid/Project/Project.h"
#include "UI/UI.h"

Store store{};
StoreHistory store_history_singleton{store}; // xxx temporary state of affairs.
StoreHistory &History = store_history_singleton;
Project MainProject{store};
// Set all global extern variables.
const Project &project = MainProject;
const fg::Style &fg::style = project.Style;
const Faust &faust = project.Audio.Faust;
const FaustGraph &faust_graph = faust.Graph;
const ImGuiSettings &imgui_settings = project.ImGuiSettings;
const FileDialog &file_dialog = project.FileDialog;
const ProjectSettings &project_settings = project.Settings;
UIContext Ui{}; // Initialize UI

bool Tick() {
    static auto &io = ImGui::GetIO();

    bool running = Ui.Tick(project);
    if (running && io.WantSaveIniSettings) {
        // ImGui sometimes sets this flags when settings have not actually changed.
        // E.g. if you press and hold a window-resize bar, it will set this flag every frame,
        // even if the cursor remains stationary (no window size change).
        // Rather than modifying the ImGui fork to not set this flag in all such cases
        // (which would likely be a rabbit hole), we just check for diffs here.
        ImGui::SaveIniSettingsToMemory(); // Populate the `Settings` context members.
        const auto &patch = imgui_settings.CreatePatch(ImGui::GetCurrentContext());
        if (!patch.Empty()) Action::Store::ApplyPatch{patch}.q();
        io.WantSaveIniSettings = false;
    }

    return running;
}

int main() {
    // Initialize the global canonical store with all project state values set during project initialization.
    store.Commit();

    // Ensure all store values set during initialization are reflected in cached field/collection values, and all side effects are run.
    Field::RefreshAll();

    IGFD::Init();

    {
        // Relying on these rendering side effects up front is not great.
        Tick(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the project state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Tick(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        Tick(); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
        RunQueuedActions(store, true);
    }

    project.OnApplicationLaunch();

    while (Tick()) {
        RunQueuedActions(store);
    }

    IGFD::Uninit();

    return 0;
}
