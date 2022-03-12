#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct toggle_demo_window {};
struct toggle_sine_wave {};
struct set_clear_color { Color color{}; };
struct set_audio_thread_running { bool running; };
struct set_sine_frequency { int frequency; };
struct set_sine_amplitude { float amplitude; };

}

using namespace action;

using Action = std::variant<
    toggle_demo_window,
    toggle_sine_wave,
    set_clear_color,
    set_audio_thread_running,
    set_sine_frequency,
    set_sine_amplitude
>;
