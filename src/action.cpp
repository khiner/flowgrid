#include "action.h"

string undo::_name = "Undo";
string redo::_name = "Redo";
string open_project::_name = "Open project";
string open_empty_project::_name = "Open empty project";
string open_default_project::_name = "Open default project";
string save_project::_name = "Save project";
string save_default_project::_name = "Save default project";
string save_current_project::_name = "Save current project";
string close_application::_name = "Close application";
string set_imgui_settings::_name = "Set ImGui Settings";
string set_imgui_style::_name = "Set ImGui Style";
string set_implot_style::_name = "Set ImPlot Style";
string set_flowgrid_style::_name = "Set FlowGrid Style";
string toggle_window::_name = "Toggle Window";
string toggle_state_viewer_auto_select::_name = "Toggle state viewer auto-select";
string set_state_viewer_label_mode::_name = "Set state-viewer label-mode";
string toggle_audio_muted::_name = "Toggle audio muted";
string set_audio_sample_rate::_name = "Set audio sample rate";
string set_faust_code::_name = "Set faust code";
string set_audio_running::_name = "Set audio running";
string toggle_audio_running::_name = "Toggle audio running";
string set_ui_running::_name = "Set UI running";

string get_action_name(const Action &action) {
    return std::visit([&](auto &&a) { return a._name; }, action);
}
