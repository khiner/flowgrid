#include "Context.h"
#include "UI/UI.h"

// Initialize global variables, and convenient shorthand variables.
Context context{};
Context &c = context;
const State &state = c.s;
const State &s = c.s;
DerivedState &ds = c.derived_state;

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

    {
        // Relying on these imperatively-run side effects up front is not great.
        c.ui = &ui_context;
        tick_ui(); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the application state reflects the fully initialized ImGui UI state (at the end of the next frame).
        tick_ui(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        c.run_queued_actions();
    }

    c.clear_undo(); // Make sure we don't start with any undo state.

    // Keep the canonical "empty" project up-to-date.
    // This project is loaded before applying diffs when loading any .fgd (FlowGridDiff) project.
    c.save_empty_project();

    // Run initialization that doesn't update state.
    // It's obvious at app start time if anything further has state-modification side effects,
    // since any further state changes would show up in the undo stack.
    c.update_faust_context();

    /** TODO need more consistent pattern for state updates.
     The issue is that by putting actions in the queue before updating state, even simple effects against state
     have an unbounded lag. This can cause race conditions, e.g.
     [this one](https://github.com/khiner/flowgrid/commit/4fee03d6d7ca35a1376b66f11e3748fe5241f99f).
     We def want a main event queue/listener so we can take expensive things like duplicating to state-json, adding to undo queue, etc.
     off of the hot path (to keep renders nice and smooth, for example).
     However, maybe we can _do the actions' state updates synchronously in the UI thread (or any actor thread),
     then signal that state has changed_, instead of signaling with the action.
     One benefit we get from queueing actions is that we guarantee some linear ordering of updates to the global `s` source of truth.
     E.g. if multiple actors (threads that generate actions) are updating the global state concurrently,
     they might step over each other and do the wrong thing.
     One way to have/eat that cake is to still decouple the actual `s` updates driven from actions from the downstream side effects
     resulting from `s` having changed, but to put the first part (updating `s`) e.g. at the end of the frame loop.

     Here are some full paths:
     1) Keep a global queue of unprocessed actions.
        Any actor can add actions to this queue.
        At the end of each frame, the UI actor pops each action in the queue and updates the state `s` synchronously.
        (In this sense, the UI actor is treated specially.)
        After updating `s`, the UI actor signals the global state has changed, and the listening global context can perform any side effects.
        This gives us the following nice properties:
        * The UI only sees one version of the global state `s` during each frame (it gets updated at most once-per-frame, at the end).
          So no race conditions resulting from updates based on different state views in a single frame.
        * We still have a synchronous, natural ordering on `s` updates, which still only happens in one place in the code.
        * We do more processing on the UI thread (the action logic, reads/writes to/from `s`), but we still do all the other heavy state processing
          side-effects on a dedicated thread. (I think this is ok.)
        Potential issues:
        * It would still be nice to allow shutting down the UI thread entirely, without killing the app, like we can currently
          (allowing for running headless).
    */

    // Merge actions that happen within very short succession.
    // This is needed e.g. to roll window size adjustments that get processed
    // by ImGui shortly after a neighboring docked window is closed.
    static const int num_action_frames_to_merge = 2;
    static int num_action_frames = 0;
    while (s.processes.ui.running) {
        tick_ui();
        bool frame_has_queued_actions = c.num_queued_actions() > 0;
        c.run_queued_actions(num_action_frames > 0);
        if (frame_has_queued_actions) num_action_frames = num_action_frames_to_merge + 1;
        num_action_frames = std::max(0, num_action_frames - 1);
    }

    destroy_ui();

    return 0;
}
