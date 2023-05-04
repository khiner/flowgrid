#include "FlowGrid/App.h"
#include "FlowGrid/Project.h"
#include "FlowGrid/StoreHistory.h"

#include "imgui.h"
#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include <filesystem>

#include <range/v3/view/map.hpp>

// Initialize global extern variables.
TransientStore InitStore{};
const State ApplicationState{};
Store ApplicationStore{InitStore.persistent()}; // Create the local canonical store, initially containing the full application state constructed by `State`.
StoreHistory History{AppStore};
UIContext UiContext{};
const State &s = ApplicationState; // Create the read-only state reference global.

int main(int, const char **) {
    InitStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.

    // Ensure all store values set during initialization are reflected in cached field/collection values.
    for (auto *field : ranges::views::values(Base::WithPath)) field->Update();

    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);

    UiContext = CreateUi(); // Initialize UI

    {
        // Relying on these imperatively-run side effects up front is not great.
        TickUi(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        TickUi(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        TickUi(); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
        Project::RunQueuedActions(true);
    }

    Project::Clear(); // Make sure we don't start with any undo state.
    Project::SaveEmptyProject(); // Keep the canonical "empty" project up-to-date.

    while (s.UiProcess.Running) {
        TickUi();
        Project::RunQueuedActions();
    }

    DestroyUi();

    return 0;
}
