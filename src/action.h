#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct set_imgui_settings { ImGuiSettings settings; };
struct set_imgui_style { ImGuiStyle imgui_style; };
struct set_implot_style { ImPlotStyle implot_style; };
struct set_flowgrid_style { FlowGridStyle flowgrid_style; };

struct toggle_window { string name; };

struct toggle_state_viewer_auto_select {};
struct set_state_viewer_label_mode { State::StateWindows::StateViewer::LabelMode label_mode; };

struct toggle_audio_muted {};
struct set_audio_sample_rate { int sample_rate; };

struct set_audio_running { bool running; };
struct toggle_audio_running {};
struct set_ui_running { bool running; };

struct set_faust_code { string text; };

struct undo {};
struct redo {};
struct open_default_project {};
struct save_default_project {};
struct close_application {};

}

using namespace action;

using Action = std::variant<
    set_imgui_settings,
    set_imgui_style,
    set_implot_style,
    set_flowgrid_style,

    toggle_window,

    toggle_state_viewer_auto_select,
    set_state_viewer_label_mode,

    toggle_audio_muted,
    set_audio_sample_rate,

    set_audio_running,
    toggle_audio_running,
    set_ui_running,

    set_faust_code,

    undo,
    redo,
    open_default_project,
    save_default_project,
    close_application
>;
