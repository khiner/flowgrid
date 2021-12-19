#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// An `Action` action stores all information needed (for a `Reducer`) to apply the action to a given `State` instance.

namespace Action {

struct set_clear_color {
    Color color{};
};

}

using namespace Action;

using action = std::variant<set_clear_color>;
