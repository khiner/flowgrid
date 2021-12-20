#pragma once

#include <vector>
#include <iostream>
#include <nlohmann/json.hpp>

#include "action.h"
#include "update.h"

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
  TODO
    Implement action listeners to:
    * Print state to stdout
    * Save JSON state to disk
    * Insert the action into the main in-memory action storage data structure
      - Almost definitely a Hash Array Mapped Trie (HAMT).
        Probably just copy (with MIT copyright notice as required)
        [this header](https://github.com/chaelim/HAMT/tree/bf7621d1ef3dfe63214db6a9293ce019fde99bcf/include),
        and modify to taste.
    * Run ImGui side-effects
 */


struct Context {
    State state{};
    std::vector<Action> actions;

    void dispatch(Action action) {
        // `update` returns a new state, without modifying the given state.
        // However, `Context.state` is read directly as a reference.
        // So for now, just assign right back to `Context.state`.
        state = update(state, action);
        json newStateJson = state;
        std::cout << newStateJson << std::endl;

        actions.push_back(action);
        std::cout << "Num actions: " << actions.size() << std::endl;
    }
};

extern Context context;
