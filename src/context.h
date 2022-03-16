#pragma once

#include <iostream>

#include "update.h"
#include "action_tree.h"
#include "state_renderers/render_json.h"

// TODO use https://github.com/cameron314/concurrentqueue for a pub-sub action event queue.
struct Context {
    const State &state = _state; // Read-only public state
    ActionTree actions;

    void dispatch(Action action) {
        update(_state, action);
        std::cout << render_json(state) << std::endl;
        actions.on_action(action);
    }

private:
    State _state{};
};

extern Context context;
