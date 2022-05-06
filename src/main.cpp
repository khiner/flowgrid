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

// TODO:
//   * Crash on undo after project open
int main(int, const char *argv[]) {
    config.app_root = std::string(argv[0]);
    config.faust_libraries_path = config.app_root + "/../../lib/faust/libraries";

    UiContext ui_context = create_ui();
    // TODO when we're loading default projects
    // if (!s.imgui_settings.empty()) s.imgui_settings.populate_context(ui_context.imgui_context);
    // else c._state.imgui_settings = ImGuiSettings(ui_context.imgui_context);

    tick_ui(ui_context); // Rendering the first frame has side effects like creating dockspaces and setting initial window sizes.
    size_t settings_size = 0;
    ImGui::SaveIniSettingsToMemory(&settings_size);

    // Synthetic actions to initialize state
    c.on_action(set_imgui_settings({ImGuiSettings(ui_context.imgui_context)})); // Initialize the ImGui state
    c.on_action(set_faust_code{s.windows.faust.code}); // Trigger faust dsp generation
    c.clear_undo(); // Make sure we don't start with any undo state (should only be the above `set_faust_code` action on the stack).
    c.state_stats = {};

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
        tick_ui(ui_context);
    }
    destroy_ui(ui_context);
    action_consumer.join();

    return 0;
}
