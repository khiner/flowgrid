#include "imgui_internal.h"

#include "FlowGrid/Core/Action/ActionQueue.h"
#include "FlowGrid/Core/Primitive/PrimitiveActionQueuer.h"
#include "FlowGrid/Core/Store/Store.h"
#include "FlowGrid/Project/FileDialog/FileDialogManager.h"
#include "FlowGrid/Project/Project.h"
#include "UI/Fonts.h"
#include "UI/UIContext.h"

Patch CreatePatch(State &state) {
    auto *ctx = ImGui::GetCurrentContext();
    auto &settings = state.ImGuiSettings;
    settings.Nodes.Set(ctx->DockContext.NodesSettings);
    settings.Windows.Set(ctx->SettingsWindows);
    settings.Tables.Set(ctx->SettingsTables);

    auto patch = state._S.CreatePatchAndResetTransient(settings.Id);
    settings.Tables.Refresh(); // xxx tables may have been modified.

    return patch;
}

bool Tick(State &state, const UIContext &ui) {
    auto &io = ImGui::GetIO();
    const bool running = ui.Tick(state);
    if (running && io.WantSaveIniSettings) {
        ImGui::SaveIniSettingsToMemory(); // Populate the `Settings` context members.
        if (auto patch = CreatePatch(state); !patch.Empty()) {
            state.Q(Action::Store::ApplyPatch{std::move(patch)});
        }
        io.WantSaveIniSettings = false;
    }

    return running;
}

int main() {
    Store store{};
    ActionQueue<Action::Any> queue{};
    const auto q_producer_token = queue.CreateProducerToken();
    ActionProducer<Action::Any>::EnqueueFn q = [&queue, &q_producer_token](auto &&a) -> bool { return queue.Enqueue(q_producer_token, std::move(a)); };
    PrimitiveActionQueuer primitive_queuer{[&q](auto &&action) -> bool { return std::visit(q, std::move(action)); }};
    Project project{store, queue.CreateConsumerToken(), primitive_queuer, q};
    State &state = *project.State;

    // Initialize the global canonical store with all project state values set during project initialization.
    store.Commit();

    // Ensure all store values set during initialization are reflected in cached field/collection values, and all side effects are run.
    state.Refresh();

    const UIContext ui{state.ImGuiSettings, state.Style}; // Initialize ImGui and other UI state.
    Fonts::Init(); // Must be done after initializing ImGui.
    ImGui::GetIO().FontGlobalScale = ui.Style.ImGui.FontScale / Fonts::AtlasScale;

    FileDialogManager::Init();

    {
        // Relying on these rendering side effects up front is not great.
        Tick(state, ui); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the project state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Tick(state, ui); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        project.ApplyQueuedActions(queue, true);
    }

    project.OnApplicationLaunch();

    static ActionMoment<Project::ActionType> action_moment; // For dequeuing to flush the queue.
    while (Tick(state, ui)) {
        // Disable all actions while the file dialog is open.
        if (state.FileDialog.Visible) {
            queue.Clear();
        } else {
            project.ApplyQueuedActions(queue, false);
        }
    }

    FileDialogManager::Uninit();

    return 0;
}
