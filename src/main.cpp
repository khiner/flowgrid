#include "FlowGrid/UI/UI.h"
#include "FlowGrid/App.h"

// Initialize global variables, and convenient shorthand variables.
Context context{};
Context &c = context;
const State &s = c.s;
const StateMap &sm = c.sm;

bool q(Action &&a, bool flush) {
    // Bailing on async action consumer for now, to avoid issues with concurrent state reads/writes, esp for json.
    // Commit dc81a9ff07e1b8e61ae6613d49183abb292abafc gets rid of the queue
    // return queue.enqueue(a);

    c.enqueue_action(a); // Actions within a single UI frame are queued up and flushed at the end of the frame (see `main.cpp`).
    if (flush) c.run_queued_actions(true); // ... unless the `flush` flag is provided, in which case we just finalize the gesture now.
    return true;
}

/**md
 # Notes

 These are things that might make their way to proper docs/readme, but need ironing out.

 ## Terminology

 * **Action:** A data structure, representing an event that can change the global state `s`.
   - An action must contain all the information needed to transform the current state into the new state after the action.
 * **Actor:** A thread that generates **actions**
 */
int main(int, const char **) {
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);

    s.Audio.update_process(); // Start audio process

    auto ui_context = create_ui();
    c.ui = &ui_context;

    {
        // Relying on these imperatively-run side effects up front is not great.
        tick_ui(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        tick_ui(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        c.run_queued_actions(true);
    }

    c.clear(); // Make sure we don't start with any undo state.

    // Keep the canonical "empty" project up-to-date.
    // This project is loaded before applying diffs when loading any .fgd (FlowGridDiff) project.
    c.save_empty_project();

    while (s.Processes.UI.Running) {
        tick_ui();
        c.run_queued_actions();
    }

    destroy_ui();

    return 0;
}
