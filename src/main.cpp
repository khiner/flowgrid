#include "imgui.h"

#include "FlowGrid/App/App.h"
#include "FlowGrid/App/ProjectConstants.h"
#include "UI/UI.h"

// Initialize global extern variables.
const App Application{}; // Initialize the global application state.
const App &app = Application; // Set the read-only app reference global.
const fg::Style &fg::style = app.Style;
const Audio &audio = app.Audio;
const AudioDevice &audio_device = audio.Device;
const Faust &faust = audio.Faust;
const FaustGraph &faust_graph = faust.Graph;
const ImGuiSettings &imgui_settings = app.ImGuiSettings;
const FileDialog &file_dialog = app.FileDialog;
const ApplicationSettings &application_settings = app.Settings;
UIContext Ui{}; // Initialize UI

int main(int, const char **) {
    // Create the global canonical store, initially containing the full application state constructed during the initialization of `App`.
    store::Commit();
    // Ensure all store values set during initialization are reflected in cached field/collection values.
    for (auto *field : ranges::views::values(Stateful::Field::Base::WithPath)) field->Update();

    Project::Init();
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);
    IGFD::Init();

    {
        // Relying on these imperatively-run side effects up front is not great.
        Ui.Tick(app); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Ui.Tick(app); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        Ui.Tick(app); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
        Project::RunQueuedActions(true);
    }

    Project::Init(); // Start with a clean slate.
    Project::SaveEmptyProject(); // Keep the canonical "empty" project up-to-date.

    while (Ui.Tick(app)) {
        Project::RunQueuedActions();
    }

    IGFD::Uninit();
    audio.Uninit();

    return 0;
}
