#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

// JSON types are used for actions that hold very large structured data.
// This is because the `Action` `std::variant` below can hold any action type, and variants must be large enough to hold their largest type.
// As of 5/24/2022, the largest raw action member type was `ImGuiStyle`, which resulted in an `Action` variant size of 1088 bytes.
// That's pretty silly for a type that can also hold a single boolean value! Replacing with JSON types brought the size down to 32 bytes.
struct set_imgui_settings { json settings; }; // ImGuiSettings
struct set_imgui_style { json imgui_style; }; // ImGuiStyle
struct set_implot_style { json implot_style; }; // ImPlotStyle
struct set_flowgrid_style { json flowgrid_style; }; // FlowGridStyle

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
struct open_project { string path; };
struct save_project { string path; };
struct open_default_project {};
struct save_default_project {};
struct close_application {};

}

using namespace action;

using Action = std::variant<
    set_imgui_settings,
    set_imgui_style, set_implot_style, set_flowgrid_style,

    toggle_window,

    toggle_state_viewer_auto_select,
    set_state_viewer_label_mode,

    toggle_audio_muted,
    set_audio_sample_rate,

    set_audio_running,
    toggle_audio_running,
    set_ui_running,

    set_faust_code,

    undo, redo,
    open_project, open_default_project,
    save_project, save_default_project,
    close_application
>;
