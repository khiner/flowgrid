#include "imgui.h"

#include "FlowGrid/App/App.h"
#include "FlowGrid/Core/Store/Store.h"
#include "UI/UI.h"

// Initialize global extern variables.
App Application{{}}; // Initialize the global application state.
const App &app = Application; // Set the read-only app reference global.
const fg::Style &fg::style = app.Style;
const AudioDevice &audio_device = app.Audio.Device;
const Faust &faust = app.Audio.Faust;
const FaustGraph &faust_graph = faust.Graph;
const ImGuiSettings &imgui_settings = app.ImGuiSettings;
const FileDialog &file_dialog = app.FileDialog;
const ApplicationSettings &application_settings = app.Settings;
UIContext Ui{}; // Initialize UI

int main() {
    // Initialize the global canonical store with all application state values set during the initialization of `App`.
    store::Commit();

    // Ensure all store values set during initialization are reflected in cached field/collection values.
    Field::RefreshAll();

    IGFD::Init();

    {
        // Relying on these rendering side effects up front is not great.
        Ui.Tick(app); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Ui.Tick(app); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        Ui.Tick(app); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
        RunQueuedActions(true);
    }

    Project::OnApplicationLaunch(); // Clean-slate project initialization.

    while (Ui.Tick(app)) {
        RunQueuedActions();
    }

    IGFD::Uninit();

    return 0;
}
