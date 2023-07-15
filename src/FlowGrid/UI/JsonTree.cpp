#include "JsonTree.h"

#include "imgui_internal.h"
#include "nlohmann/json.hpp"

using std::string;
using namespace ImGui;
using json = nlohmann::json;

namespace FlowGrid {
bool TreeNode(std::string_view label_view, const char *id, const char *value) {
    bool is_open = false;
    if (value == nullptr) {
        const auto label = string(label_view);
        is_open = id ? TreeNodeEx(id, ImGuiTreeNodeFlags_None, "%s", label.c_str()) : TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_None);
    } else if (!label_view.empty()) {
        Text("%s: ", string(label_view).c_str()); // Render leaf label/value as raw text.
    }

    if (value != nullptr) {
        SameLine();
        TextUnformatted(value);
    }
    return is_open;
}

void JsonTree(std::string_view label, json &&value, const char *id) {
    if (value.is_null()) {
        TextUnformatted(label.empty() ? "(null)" : string(label).c_str());
    } else if (value.is_object()) {
        if (label.empty() || TreeNode(label, id)) {
            for (auto it : value.items()) {
                JsonTree(it.key(), std::move(it.value()));
            }
            if (!label.empty()) TreePop();
        }
    } else if (value.is_array()) {
        if (label.empty() || TreeNode(label, id)) {
            unsigned int i = 0;
            for (auto it : value) {
                JsonTree(std::to_string(i), std::move(it));
                i++;
            }
            if (!label.empty()) TreePop();
        }
    } else {
        TreeNode(label, id, std::move(value).dump().c_str());
    }
}
} // namespace FlowGrid
