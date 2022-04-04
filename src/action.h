#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct undo {};
struct redo {};

struct set_ini_settings { std::string settings; };
struct set_style { ImGuiStyle style; };

struct toggle_window { std::string name; };

struct toggle_audio_muted {};
struct set_audio_thread_running { bool running; };
struct toggle_audio_running {};
struct set_audio_sample_rate { int sample_rate; };

struct set_action_consumer_running { bool running; };
struct set_ui_running { bool running; };

struct set_faust_text { std::string text; };
struct toggle_faust_simple_text_editor {};

struct close_application {};

}

using namespace action;

using Action = std::variant<
    undo,
    redo,

    set_ini_settings,
    set_style,

    toggle_window,

    toggle_audio_muted,
    set_audio_thread_running,
    toggle_audio_running,
    set_audio_sample_rate,

    set_action_consumer_running,
    set_ui_running,

    set_faust_text,
    toggle_faust_simple_text_editor,

    close_application
>;
