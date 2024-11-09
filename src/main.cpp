#include "imgui_internal.h"
#include "implot.h"

#include "Core/FileDialog/FileDialogManager.h"
#include "Core/Project/Project.h"
#include "Core/UI/Fonts.h"
#include "Core/UI/UIContext.h"

#include "FlowGrid.h"

int main() {
    Project project{[](auto app_args) { return std::make_unique<FlowGrid>(std::move(app_args)); }};
    auto &state = project.Core;

    auto predraw = [&state]() {
        // Check if new UI settings need to be applied.
        auto &settings = state.ImGuiSettings;
        auto &style = state.Style;
        settings.UpdateIfChanged(ImGui::GetCurrentContext());
        style.ImGui.UpdateIfChanged(ImGui::GetCurrentContext());
        style.ImPlot.UpdateIfChanged(ImPlot::GetCurrentContext());

        // Check if new font settings need to be applied.
        auto &io = ImGui::GetIO();
        static int PrevFontIndex = 0;
        static float PrevFontScale = 1.0;
        if (PrevFontIndex != style.ImGui.FontIndex) {
            io.FontDefault = io.Fonts->Fonts[style.ImGui.FontIndex];
            PrevFontIndex = style.ImGui.FontIndex;
        }
        if (PrevFontScale != style.ImGui.FontScale) {
            io.FontGlobalScale = style.ImGui.FontScale / Fonts::AtlasScale;
            PrevFontScale = style.ImGui.FontScale;
        }
    };
    auto draw = [&project]() { project.Draw(); };
    const UIContext ui{std::move(predraw), std::move(draw)};
    Fonts::Init(); // Must be done after initializing ImGui.
    ImGui::GetIO().FontGlobalScale = state.Style.ImGui.FontScale / Fonts::AtlasScale;

    FileDialogManager::Init();

    {
        // Relying on these rendering side effects up front is not great.
        ui.Tick(); // Rendering the first frame has side effects like creating dockspaces & windows.
        project.Tick();
        ImGui::GetIO().WantSaveIniSettings = true; // Make sure the project state reflects the fully initialized ImGui UI state (at the end of the next frame).
        ui.Tick(); // Another frame is needed for ImGui to update its Window->DockNode relationships after creating the windows in the first frame.
        project.Tick();
        project.ApplyQueuedActions(true);
    }

    project.OnApplicationLaunch();
    while (ui.Tick()) {
        project.Tick();
    }

    FileDialogManager::Uninit();

    return 0;
}
