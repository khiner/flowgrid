#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct undo {};
struct redo {};

struct set_imgui_settings { ImGuiSettings settings; };
struct set_imgui_style { ImGuiStyle imgui_style; };
struct set_implot_style { ImPlotStyle implot_style; };

struct toggle_window { std::string name; };

struct set_state_viewer_label_mode { Windows::StateWindows::StateViewer::Settings::LabelMode label_mode; };

struct toggle_audio_muted {};
struct set_audio_thread_running { bool running; };
struct toggle_audio_running {};
struct set_audio_sample_rate { int sample_rate; };

struct set_ui_running { bool running; };

struct set_faust_code { std::string text; };

struct close_application {};

}

using namespace action;

using Action = std::variant<
    undo,
    redo,

    set_imgui_settings,
    set_imgui_style,
    set_implot_style,

    toggle_window,

    set_state_viewer_label_mode,

    toggle_audio_muted,
    set_audio_thread_running,
    toggle_audio_running,
    set_audio_sample_rate,

    set_ui_running,

    set_faust_code,

    close_application
>;
