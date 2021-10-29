#pragma once

#include "state.h"

State &do_noop(State &state) {
    return &state;
}
