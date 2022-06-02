#include "action.h"

const string undo::_name = "Undo";
const string redo::_name = "Redo";
const string open_project::_name = "Open project";
const string open_empty_project::_name = "Open empty project";
const string open_default_project::_name = "Open default project";
const string save_project::_name = "Save project";
const string save_default_project::_name = "Save default project";
const string save_current_project::_name = "Save current project";
const string close_application::_name = "Close application";
const string set_imgui_settings::_name = "Set ImGui Settings";
const string set_imgui_style::_name = "Set ImGui Style";
const string set_implot_style::_name = "Set ImPlot Style";
const string set_flowgrid_style::_name = "Set FlowGrid Style";
const string toggle_window::_name = "Toggle Window";
const string toggle_state_viewer_auto_select::_name = "Toggle state viewer auto-select";
const string set_state_viewer_label_mode::_name = "Set state-viewer label-mode";
const string toggle_audio_muted::_name = "Toggle audio muted";
const string set_audio_sample_rate::_name = "Set audio sample rate";
const string set_faust_code::_name = "Set faust code";
const string set_audio_running::_name = "Set audio running";
const string toggle_audio_running::_name = "Toggle audio running";
const string set_ui_running::_name = "Set UI running";

ActionId get_action_id(const Action &action) {
    return action.index();
}
string get_action_name(const Action &action) {
    return std::visit([&](auto &&a) { return a._name; }, action);
}
