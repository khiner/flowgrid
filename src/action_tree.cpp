#include <iostream>
#include "action_tree.h"

void ActionTree::on_action(const Action &action, const json &diff) {
    actions.emplace_back(std::make_pair(action, diff));
    std::cout << "Action #" << actions.size() << " diff: " << actions.back().second << std::endl;
}
