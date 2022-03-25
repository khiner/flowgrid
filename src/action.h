#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct undo {};
struct redo {};

// TODO `toggle_window{ string name }`
struct toggle_demo_window {};
struct toggle_faust_editor_window {};
struct toggle_faust_editor_open {};

struct toggle_audio_muted {};
struct set_clear_color { Color color{}; };

struct set_audio_thread_running { bool running; };
struct toggle_audio_running {};
struct set_action_consumer_running { bool running; };
struct set_ui_running { bool running; };
struct set_faust_text { std::string text; };
struct set_audio_sample_rate { int sample_rate; };

struct close_application {};

}

using namespace action;

using Action = std::variant<
    undo,
    redo,

    toggle_demo_window,
    toggle_faust_editor_window,
    toggle_faust_editor_open,

    toggle_audio_muted,
    set_clear_color,

    set_audio_thread_running,
    toggle_audio_running,
    set_action_consumer_running,
    set_ui_running,
    set_faust_text,
    set_audio_sample_rate,

    close_application
>;
