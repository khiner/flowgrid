#include "../windows.h"
#include "../../context.h"

static void add_json_value_node(const std::string &key, json &value) {
    //      ImGuiTreeNodeFlags_DefaultOpen or SetNextItemOpen()
    if (value.is_object()) {
        if (ImGui::TreeNode(key.c_str())) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                add_json_value_node(it.key(), it.value());
            }
            ImGui::TreePop();
        }
    } else if (value.is_array()) {
        if (ImGui::TreeNode(key.c_str())) {
            int i = 0;
            for (auto it: value) {
                add_json_value_node(std::to_string(i++), it);
            }
            ImGui::TreePop();
        }
    } else {
        std::string text = key + " : " + value.dump();
        if (ImGui::TreeNode(text.c_str())) {
            ImGui::TreePop();
        }
    }
}

void StateViewer::draw(Window &) {
    add_json_value_node("State", c.json_state);
}
