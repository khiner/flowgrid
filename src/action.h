#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct toggle_demo_window {};
struct toggle_audio_muted {};
struct set_clear_color { Color color{}; };
struct set_audio_thread_running { bool running; };
struct set_action_consumer_running { bool running; };
struct set_ui_running { bool running; };
struct close_application {};

}

using namespace action;

using Action = std::variant<
    toggle_demo_window,
    toggle_audio_muted,
    set_clear_color,
    set_audio_thread_running,
    set_action_consumer_running,
    set_ui_running,
    close_application
>;
