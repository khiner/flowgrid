#include <thread>
#include "context.h"
#include "draw.h"
#include "process_manager.h"

// Initialize global variables, and convenient shorthand variables.
Config config{};
Context context{};
Context &c = context;
const State &state = c.s;
const State &s = c.s;
State &ui_s = c.ui_s;
BlockingConcurrentQueue<Action> queue{}; // NOLINT(cppcoreguidelines-interfaces-global-init)

/**md
 # Notes

 These are things that might make their way to proper docs/readme, but need ironing out.

 ## Terminology

 * **Action:** A data structure, representing an event that can change the global state `s`.
   - An action must contain all the information needed to transform the current state into the new state after the action.
 * **Actor:** A thread that generates **actions**
 */
int main(int, const char *argv[]) {
    config.app_root = std::string(argv[0]);
    config.faust_libraries_path = config.app_root + "/../../lib/faust/libraries";
    auto ui_context = create_ui();
    context.ui = &ui_context;

    // TODO when we're loading default projects
    // if (!s.imgui_settings.empty()) s.imgui_settings.populate_context(ui_context.imgui_context);
    // else c._state.imgui_settings = ImGuiSettings(ui_context.imgui_context);

    tick_ui(); // Rendering the first frame has side effects like creating dockspaces & windows.
    tick_ui(); // Another frame is needed to update the Window->DockNode relationships after creating the windows in the first frame.

    // Synthetic actions to initialize state
    c.on_action(set_imgui_settings({ImGuiSettings(c.ui->imgui_context)})); // Initialize the ImGui state
    c.on_action(set_faust_code{s.windows.faust.code}); // Trigger faust dsp generation
    c.clear_undo(); // Make sure we don't start with any undo state (should only be the above `set_faust_code` action on the stack).
    c.state_stats = {};

    /** TODO need more consistent pattern for state updates.
     The issue is that by putting actions in the queue before updating state, even simple effects against state
     have an unbounded lag. This can cause race conditions, e.g.
     [this one](https://github.com/khiner/flowgrid2/commit/4fee03d6d7ca35a1376b66f11e3748fe5241f99f).
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

      Ok I think I know a next step to try first - can we get rid of the `ui_s` mutable state copy entirely?
    */
    ProcessManager process_manager;
    std::thread action_consumer([&]() {
        while (s.processes.action_consumer.running) {
            Action a;
            queue.wait_dequeue(a);
            c.on_action(a);
            process_manager.on_action(a);
        }
    });

    while (s.processes.ui.running) {
        tick_ui();
    }
    destroy_ui();
    action_consumer.join();

    return 0;
}
