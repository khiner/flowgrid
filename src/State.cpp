#include "State.h"

ImGuiSettings::ImGuiSettings(ImGuiContext *c) {
    ImGui::SaveIniSettingsToMemory(); // Populates the `Settings` context members
    nodes = c->DockContext.NodesSettings; // already an ImVector
    // Convert `ImChunkStream`s to `ImVector`s.
    for (auto *ws = c->SettingsWindows.begin(); ws != nullptr; ws = c->SettingsWindows.next_chunk(ws)) {
        windows.push_back(*ws);
    }
    for (auto *ts = c->SettingsTables.begin(); ts != nullptr; ts = c->SettingsTables.next_chunk(ts)) {
        tables.push_back(*ts);
    }
}

// Copied from `imgui.cpp`
static void ApplyWindowSettings(ImGuiWindow *window, ImGuiWindowSettings *settings) {
    if (!window) return; // TODO log

    const ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    window->ViewportPos = main_viewport->Pos;
    if (settings->ViewportId) {
        window->ViewportId = settings->ViewportId;
        window->ViewportPos = ImVec2(settings->ViewportPos.x, settings->ViewportPos.y);
    }
    window->Pos = ImFloor(ImVec2(settings->Pos.x + window->ViewportPos.x, settings->Pos.y + window->ViewportPos.y));
    if (settings->Size.x > 0 && settings->Size.y > 0)
        window->Size = window->SizeFull = ImFloor(ImVec2(settings->Size.x, settings->Size.y));
    window->Collapsed = settings->Collapsed;
    window->DockId = settings->DockId;
    window->DockOrder = settings->DockOrder;
}

void ImGuiSettings::populate_context(ImGuiContext *c) const {
    /** Clear **/
    ImGui::DockSettingsHandler_ClearAll(c, nullptr);

    /** Apply **/
    for (auto ws: windows) ApplyWindowSettings(ImGui::FindWindowByID(ws.ID), &ws);

    c->DockContext.NodesSettings = nodes; // already an ImVector
    ImGui::DockSettingsHandler_ApplyAll(c, nullptr);

    /** Other housekeeping to emulate `ImGui::LoadIniSettingsFromMemory` **/
    c->SettingsLoaded = true;
    c->SettingsDirty = false;
}
