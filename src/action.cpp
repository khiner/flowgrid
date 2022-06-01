#include "action.h"

string get_action_name(const Action &action) {
    return std::visit(visitor{
        [&](const set_imgui_settings &) { return "Set ImGui Settings"; },
        [&](const set_imgui_style &) { return "Set ImGui Style"; },
        [&](const set_implot_style &) { return "Set ImPlot Style"; },
        [&](const set_flowgrid_style &) { return "Set FlowGrid Style"; },

        [&](const toggle_window &) { return "Toggle Window"; },

        [&](const toggle_state_viewer_auto_select &) { return "Toggle state viewer auto-select"; },
        [&](const set_state_viewer_label_mode &) { return "Set state-viewer label-mode"; },
        [&](const toggle_audio_muted &) { return "Toggle audio muted"; },
        [&](const set_audio_sample_rate &) { return "Set audio sample rate"; },
        [&](const set_faust_code &) { return "Set faust code"; },

        [&](const set_audio_running &) { return "Set audio running"; },
        [&](const toggle_audio_running &) { return "Toggle audio running"; },
        [&](const set_ui_running &) { return "Set UI running"; },

        [&](const undo &) { return "Undo"; },
        [&](const redo &) { return "Redo"; },
        [&](const open_project &) { return "Open project"; },
        [&](const open_empty_project &) { return "Open empty project"; },
        [&](const open_default_project &) { return "Open default project"; },
        [&](const save_project &) { return "Save project"; },
        [&](const save_default_project &) { return "Save default project"; },
        [&](const save_current_project &) { return "Save current project"; },
        [&](const close_application &) { return "Close application"; },
    }, action);
}
