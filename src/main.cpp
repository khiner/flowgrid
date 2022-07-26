#include "UI/UI.h"
#include "Context.h"

// Initialize global variables, and convenient shorthand variables.
Context context{};
Context &c = context;
const State &s = c.s;
const json &sj = c.sj;

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

    c.update_processes(); // Currently has a state side effect of setting audio sample rate.

    auto ui_context = create_ui();
    c.ui = &ui_context;

    {
        // Relying on these imperatively-run side effects up front is not great.
        tick_ui(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        tick_ui(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        c.run_queued_actions();
    }

    c.init(); // Make sure we don't start with any undo state.

    // Keep the canonical "empty" project up-to-date.
    // This project is loaded before applying diffs when loading any .fgd (FlowGridDiff) project.
    c.save_empty_project();

    // Run initialization that doesn't update state.
    // It's obvious at app start time if anything further has state-modification side effects,
    // since any further state changes would show up in the undo stack.
    c.update_faust_context();

    // Merge actions that happen within very short succession.
    // This is needed e.g. to roll window size adjustments that get processed by ImGui shortly after a neighboring docked window is closed.
    static const int num_action_frames_to_merge = 3;
    static int num_action_frames = 0;
    while (s.processes.ui.running) {
        tick_ui();
        const bool frame_has_queued_actions = c.num_queued_actions() > 0;
        c.run_queued_actions(num_action_frames > 0);
        if (frame_has_queued_actions) num_action_frames = num_action_frames_to_merge + 1;
        num_action_frames = std::max(0, num_action_frames - 1);
    }

    destroy_ui();

    return 0;
}
