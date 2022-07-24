#include "ImGuiFileDialog.h"

#include "State.h"
#include "Action.h"
#include "File.h"

/**
 * Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer), but only the action-visitor pattern remains.
 */
void State::update(const Action &action) {
    std::visit(visitor{
        [&](const show_open_project_dialog &) { file.dialog = {"Choose file", AllProjectExtensionsDelimited, "."}; },
        [&](const show_save_project_dialog &) { file.dialog = {"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_open_faust_file_dialog &) { file.dialog = {"Choose file", FaustDspFileExtension, "."}; },
        [&](const show_save_faust_file_dialog &) { file.dialog = {"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },

        [&](const open_file_dialog &a) {
            file.dialog = a.dialog;
            file.dialog.visible = true;
        },
        [&](const close_file_dialog &) { file.dialog.visible = false; },

        [&](const set_imgui_settings &a) { imgui_settings = a.settings; },
        [&](const set_imgui_color_style &a) {
            auto *dst = &style.imgui;
            switch (a.id) {
                case 0: ImGui::StyleColorsDark(dst);
                    break;
                case 1: ImGui::StyleColorsLight(dst);
                    break;
                case 2: ImGui::StyleColorsClassic(dst);
                    break;
            }
        },
        [&](const set_implot_color_style &a) {
            auto *dst = &style.implot;
            switch (a.id) {
                case 0: ImPlot::StyleColorsAuto(dst);
                    break;
                case 1: ImPlot::StyleColorsClassic(dst);
                    break;
                case 2: ImPlot::StyleColorsDark(dst);
                    break;
                case 3: ImPlot::StyleColorsLight(dst);
                    break;
            }
        },
        [&](const set_flowgrid_color_style &a) {
            auto &dst = style.flowgrid;
            switch (a.id) {
                case 0: FlowGridStyle::StyleColorsDark(dst);
                    break;
                case 1: FlowGridStyle::StyleColorsLight(dst);
                    break;
                case 2: FlowGridStyle::StyleColorsClassic(dst);
                    break;
                default:break;
            }
        },

        [&](const close_window &a) { window_named.at(a.name).get().visible = false; },
        [&](const toggle_window &a) { window_named.at(a.name).get().visible = !window_named.at(a.name).get().visible; },

        [&](const toggle_state_viewer_auto_select &) { state_viewer.auto_select = !state_viewer.auto_select; },
        [&](const set_state_viewer_label_mode &a) { state_viewer.label_mode = a.label_mode; },

        // Audio
        [&](const open_faust_file &a) { audio.faust.code = ::File::read(a.path); },
        [&](const set_faust_code &a) { audio.faust.code = a.text; },
        [&](const set_audio_sample_rate &a) { audio.settings.sample_rate = a.sample_rate; },

        [&](const set_ui_running &a) { processes.ui.running = a.running; },

        [&](const close_application &) {
            processes.ui.running = false;
            processes.audio.running = false;
        },

        [&](const auto &) {}, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

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
