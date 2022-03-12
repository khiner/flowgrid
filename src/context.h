#pragma once

#include <iostream>
#include <nlohmann/json.hpp>

#include "action.h"
#include "update.h"
#include "action_tree.h"

using json = nlohmann::json;

/**
  TODO a pub-sub action event queue.
    See [lager](https://github.com/arximboldi/lager/blob/master/lager/context.hpp), which is very complicated.
    What we need from `lager::context` is the ability to subscribe to a filtered set of actions.
    The GOAT: https://github.com/arximboldi/lager/blob/master/lager/context.hpp
    Basically I want this pattern ^, but want to implement it as needed myself, and fully understand it.
    Other resources:
    * https://www.geeksforgeeks.org/sharing-queue-among-three-threads/
    * https://www.codeproject.com/Articles/1169105/Cplusplus-std-thread-Event-Loop-with-Message-Queu
 */

struct Context {
    State state{};
    ActionTree actions;

    void dispatch(Action action) {
        state = update(state, action);
        json newStateJson = state;
        std::cout << newStateJson << std::endl;
        actions.on_action(action);
    }
};

extern Context context;
