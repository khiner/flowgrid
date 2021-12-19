#pragma once

#include <vector>
#include <iostream>
#include <nlohmann/json.hpp>

#include "action.h"
#include "update.h"

using json = nlohmann::json;

/**
 * TODO:
 *   * move the state "play-head" backward or forward in time by some `int steps`
 *     - this method steps through each diff one by one
 *   * take full-state snapshots periodically (or explicitly via a method)
 *   * compress (sequentially combine) any contiguous range of the series to a shorter length (down to length 1)
 *     - provide both mutating and non-mutating versions
 *       * mutating permanently destroys intermediate states between the two requested states
 *         - used to limit memory usage, to an optionally-specified hard memory limit
 *         - support two modes: "drop tail", or "drop middle"
 *           * What's more important to you? The full story from beginning to end, or fidelity of the recent past? Some combination?
 *       * non-mutating returns the requested range
 *   Note: A diff-first implementation means random-access is $O(L)$, where $L$ is the length of the state series (the total number of actions).
  *      However, performance can be bought with more storage by storing more snapshots.
  *      With more snapshots, any position can be accessed more quickly by first skipping to the state snapshot closest to the requested index.
 */

struct Context {
    State state{};
    std::vector<Action> actions;

    void dispatch(Action action) {
        // `update` returns a new state, without modifying the given state.
        // However, `Context.state` is read directly as a reference.
        // So for now, just assign right back to `Context.state`.
        state = update(state, action);
        // TODO save json to file instead of printing.
        json newStateJson = state;
        std::cout << newStateJson << std::endl;

        actions.push_back(action);
        std::cout << "Num actions: " << actions.size() << std::endl;
    }
};
