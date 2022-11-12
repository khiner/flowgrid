#include "FlowGrid/UI/UI.h"
#include "FlowGrid/App.h"

// Initialize global variables, and convenient shorthand variables.
TransientStore application_ctor_store{}; // Create the local transient store used only for `State` construction...
TransientStore &ctor_store = application_ctor_store; // ... and assign to its mutable, read-write reference global.

State application_state{}; // Create the transient store used only for `State` construction...
const State &state = application_state; // ... and assign to its immutable, read-only reference global...
const State &s = application_state; // ... and a convenient single-letter duplicate reference.

Store application_store = application_ctor_store.persistent(); // Create the local canonical store, initially containing the full application state constructed by `State`...
const Store &store = application_store; // ... and assign to its immutable, read-only reference global.

UIContext UiContext{};

Context application_context{};
Context &context = application_context;
Context &c = application_context;

Store SetStore(Store persistent) { return application_store = std::move(persistent); }

bool q(Action &&a, bool flush) {
    c.EnqueueAction(a); // Actions within a single UI frame are queued up and flushed at the end of the frame.
    if (flush) c.RunQueuedActions(true); // ... unless the `flush` flag is provided, in which case we just finalize the gesture now.
    return true;
}

int main(int, const char **) {
    application_ctor_store = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);

    UiContext = CreateUi(); // Initialize UI
    s.Audio.UpdateProcess(); // Start audio process
    s.ApplicationSettings.ActionConsumer.UpdateProcess(); // Start action consumer

    {
        // Relying on these imperatively-run side effects up front is not great.
        TickUi(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        TickUi(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        c.RunQueuedActions(true);
    }

    c.Clear(); // Make sure we don't start with any undo state.
    c.SaveEmptyProject(); // Keep the canonical "empty" project up-to-date.

    while (s.UiProcess.Running) {
        TickUi();
        c.RunQueuedActions();
    }

    DestroyUi();

    return 0;
}
