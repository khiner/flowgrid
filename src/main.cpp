#include "imgui_internal.h"

#include "FlowGrid/Core/Primitive/PrimitiveActionQueuer.h"
#include "FlowGrid/Core/Store/Store.h"
#include "FlowGrid/Project/FileDialog/FileDialogManager.h"
#include "FlowGrid/Project/Project.h"
#include "UI/Fonts.h"
#include "UI/UIContext.h"

Patch CreatePatch(Project &project) {
    auto *ctx = ImGui::GetCurrentContext();
    auto &settings = project.ImGuiSettings;
    settings.Nodes.Set(ctx->DockContext.NodesSettings);
    settings.Windows.Set(ctx->SettingsWindows);
    settings.Tables.Set(ctx->SettingsTables);

    auto patch = settings.S.CreatePatchAndResetTransient(settings.Id);
    settings.Tables.Refresh(); // xxx tables may have been modified.

    return patch;
}

bool Tick(Project &project, const UIContext &ui) {
    auto &io = ImGui::GetIO();

    const bool running = ui.Tick(project);
    if (running && io.WantSaveIniSettings) {
        ImGui::SaveIniSettingsToMemory(); // Populate the `Settings` context members.
        if (auto patch = CreatePatch(project); !patch.Empty()) {
            project.Q(Action::Store::ApplyPatch{std::move(patch)});
        }
        io.WantSaveIniSettings = false;
    }

    return running;
}

int main() {
    Store store{};
    ActionQueue<Action::Any> queue{};
    ActionProducer<Action::Any>::EnqueueFn q = [&queue](auto &&a) -> bool { return queue.Enqueue(std::move(a)); };
    PrimitiveActionQueuer primitive_queuer{[&q](auto &&action) -> bool { return std::visit(q, std::move(action)); }};
    Project project{store, primitive_queuer, q};

    // Initialize the global canonical store with all project state values set during project initialization.
    store.Commit();

    // Ensure all store values set during initialization are reflected in cached field/collection values, and all side effects are run.
    Component::RefreshAll();

    const UIContext ui{project.ImGuiSettings, project.Style}; // Initialize ImGui and other UI state.
    Fonts::Init(); // Must be done after initializing ImGui.
    ImGui::GetIO().FontGlobalScale = ui.Style.ImGui.FontScale / Fonts::AtlasScale;

    FileDialogManager::Init();

    {
        // Relying on these rendering side effects up front is not great.
        Tick(project, ui); // Rendering the first frame has side effects like creating dockspaces & windows.
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the project state reflects the fully initialized ImGui UI state (at the end of the next frame).
        Tick(project, ui); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        project.ApplyQueuedActions(queue, true);
    }

    project.OnApplicationLaunch();

    while (Tick(project, ui)) {
        // Disable all actions while the file dialog is open.
        project.ApplyQueuedActions(queue, false, project.FileDialog.Visible);
    }

    FileDialogManager::Uninit();

    return 0;
}
