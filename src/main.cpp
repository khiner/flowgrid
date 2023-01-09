#include "FlowGrid/App.h"

// Create global, mutable `UIContext` and `Context` instances.
UIContext UiContext{};
Context c{};
const Store &AppStore = c.AppStore; // Create the read-only store reference global.
const State &s = c.s; // Create the read-only state reference global.

int main(int, const char **) {
    // It's here to make sure that any changes to `c.InitStore` _after_ field construction are reflected in the cached field value.
    for (auto *field : views::values(Base::WithPath)) field->Update();
    for (auto *field : views::values(UntypedVector::WithPath)) field->Update();
    for (auto *field : views::values(UntypedVector2D::WithPath)) field->Update();

    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);

    UiContext = CreateUi(); // Initialize UI
    s.Audio.Init(); // Start audio process

    {
        // Relying on these imperatively-run side effects up front is not great.
        TickUi(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        TickUi(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        TickUi(); // Another one seems to be needed to update selected tabs? (I think this happens when changes during initilization change scroll position or somesuch.)
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
