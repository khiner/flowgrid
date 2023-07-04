#include "imgui.h"

#include "FlowGrid/Core/Store/Store.h"
#include "FlowGrid/Project/Project.h"
#include "UI/UI.h"

Project MainProject{{}};
// Set all global extern variables.
const Project &project = MainProject;
const fg::Style &fg::style = project.Style;
const AudioDevice &audio_device = project.Audio.Device;
const Faust &faust = project.Audio.Faust;
const FaustGraph &faust_graph = faust.Graph;
const ImGuiSettings &imgui_settings = project.ImGuiSettings;
const FileDialog &file_dialog = project.FileDialog;
const ProjectSettings &project_settings = project.Settings;
UIContext Ui{}; // Initialize UI

int main() {
    // Initialize the global canonical store with all project state values set during project initialization.
    store::Commit();

    // Ensure all store values set during initialization are reflected in cached field/collection values, and all side effects are run.
    Field::RefreshAll();

    IGFD::Init();

    {
        // Relying on these rendering side effects up front is not great.
        Ui.Tick(project); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the project state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Ui.Tick(project); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        Ui.Tick(project); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
        RunQueuedActions(true);
    }

    Project::OnApplicationLaunch();

    while (Ui.Tick(project)) {
        RunQueuedActions();
    }

    IGFD::Uninit();

    return 0;
}
