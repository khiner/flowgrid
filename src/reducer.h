#pragma once

#include "state.h"

State app_reducer(State state, action) {
    switch (action) {
        default:
            // If this reducer doesn't recognize the action type, or doesn't
            // care about this specific action, return the existing state unchanged
            return state
    }
}
