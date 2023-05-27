#include "FlowGrid/App/App.h"
#include "FlowGrid/App/ProjectConstants.h"

#include "imgui.h"
#include <filesystem>

// Initialize global extern variables.
const State ApplicationState{};
const State &s = ApplicationState; // Create the read-only state reference global.
const fg::Style &fg::style = s.Style;
const Audio &audio = s.Audio;
const ImGuiSettings &imgui_settings = s.ImGuiSettings;
const FileDialog &file_dialog = s.FileDialog;
const ApplicationSettings &application_settings = s.Settings;

UIContext UiContext{};

int main(int, const char **) {
    // Create the global canonical store, initially containing the full application state constructed by `State`.
    store::CommitTransient();
    // Ensure all store values set during initialization are reflected in cached field/collection values.
    for (auto *field : ranges::views::values(Stateful::Field::Base::WithPath)) field->Update();

    Project::Init(); // Start project.

    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);

    UiContext = CreateUi(); // Initialize UI

    {
        // Relying on these imperatively-run side effects up front is not great.
        TickUi(s); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        TickUi(s); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        TickUi(s); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
        Project::RunQueuedActions(true);
    }

    Project::Init(); // Start with a clean slate.
    Project::SaveEmptyProject(); // Keep the canonical "empty" project up-to-date.

    while (s.Running) {
        TickUi(s);
        Project::RunQueuedActions();
    }

    DestroyUi();

    return 0;
}
