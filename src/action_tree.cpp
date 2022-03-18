#include <iostream>
#include "action_tree.h"

void ActionTree::on_action(Action &action, const json &diff) {
    actions.emplace_back(action, diff);
    std::cout << "Action #" << actions.size() << " diff: " << actions.back().second << std::endl;
}
