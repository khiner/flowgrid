#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// An `Action` action stores all information needed (for a `Reducer`) to apply the action to a given `State` instance.

namespace action {

struct set_clear_color {
    Color color{};
};

struct set_audio_engine_running {
    bool running;
};

}

using namespace action;

using Action = std::variant<set_clear_color, set_audio_engine_running>;
