#include <iostream>
#include "action_tree.h"

void ActionTree::on_action(Action &action) {
    actions.push_back(action);
    std::cout << "Num actions: " << actions.size() << std::endl;
}
