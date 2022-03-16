#pragma once

#include <iostream>

#include "update.h"
#include "action_tree.h"
#include "state_renderers/render_json.h"

// TODO use https://github.com/cameron314/concurrentqueue for a pub-sub action event queue.
// TODO Enforce read-only direct access to state: https://stackoverflow.com/a/45996145/780425
struct Context {
    State state{};
    ActionTree actions;

    void dispatch(Action action) {
        state = update(state, action);
        std::cout << render_json(state) << std::endl;
        actions.on_action(action);
    }
};

extern Context context;
