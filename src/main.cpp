#include "imgui_internal.h"
#include "implot.h"

#include "Core/FileDialog/FileDialogManager.h"
#include "Core/Project/Project.h"
#include "Core/UI/Fonts.h"
#include "Core/UI/UIContext.h"

#include "FlowGrid.h"

int main() {
    Project project{[](auto app_args) { return std::make_unique<FlowGrid>(std::move(app_args)); }};
    const auto &core = project.Core;

    auto predraw = [&core]() {
        // Check if new UI settings need to be applied.
        auto &style = core.Style;
        core.ImGuiSettings.UpdateIfChanged(ImGui::GetCurrentContext());
        style.ImGui.UpdateIfChanged(ImGui::GetCurrentContext());
        style.ImPlot.UpdateIfChanged(ImPlot::GetCurrentContext());

        Fonts::Tick(style.ImGui.FontScale, style.ImGui.FontIndex);
    };

    auto draw = [&project]() { project.Draw(); };
    const UIContext ui{std::move(predraw), std::move(draw)};
    Fonts::Init(core.Style.ImGui.FontScale);
    FileDialogManager::Init();

    // Initial rendering has state-modifying (action-producing) side effects.
    {
        // First frame creates dockspaces & windows.
        ui.Tick();
        // After creating the windows, another frame is needed for ImGui to update its Window->DockNode relationships.
        ImGui::GetIO().WantSaveIniSettings = true; // Ensure project state reflects the fully-initialized ImGui state at the end of the next frame.
        ui.Tick();
    }

    project.Init();
    while (ui.Tick()) {
        project.Tick();
    }

    FileDialogManager::Uninit();

    return 0;
}
