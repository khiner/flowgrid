#pragma once

#include <vector>
#include "action.h"

/**
This is a placeholder for the main in-memory data structure for action history.
- Undo should have similar functionality to [Vim's undotree](https://github.com/mbbill/undotree/blob/master/autoload/undotree.vim)
  - Consider the Hash Array Mapped Trie (HAMT) data structure for state, diff, and/or actions (fast keyed access and fast-ish updates,
    exploiting the state's natural tree structure.
  - Probably just copy (with MIT copyright notice as required)
    [this header](https://github.com/chaelim/HAMT/tree/bf7621d1ef3dfe63214db6a9293ce019fde99bcf/include),
    and modify to taste.
*/
struct ActionTree {
    void on_action(Action &);
    std::vector<Action> actions;
};
