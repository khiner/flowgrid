#include "Widgets.h"
#include "../Context.h"

void fg::gestured() {
    if (ImGui::IsItemActivated()) c.is_widget_gesturing = true;
    if (ImGui::IsItemDeactivated()) c.is_widget_gesturing = false;
}

bool fg::ColorEdit4(const JsonPath &path, ImGuiColorEditFlags flags, const char *label) {
    ImVec4 v = sj[path];
    const bool edited = ImGui::ColorEdit4(label ? label : path_label(path).c_str(), (float *) &v, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

void fg::MenuItem(ActionID action_id) {
    const char *menu_label = action::get_menu_label(action_id);
    const char *shortcut = action::shortcut_for_id.contains(action_id) ? action::shortcut_for_id.at(action_id).c_str() : nullptr;
    if (ImGui::MenuItem(menu_label, shortcut, false, c.action_allowed(action_id))) q(action::create(action_id));
}

void fg::ToggleMenuItem(const StateMember &member) {
    const string &menu_label = path_label(member.Path);
    if (ImGui::MenuItem(menu_label.c_str(), nullptr, sj[member.Path])) q(toggle_value{member.Path});
}

bool fg::JsonTreeNode(const string &label, JsonTreeNodeFlags flags, const char *id) {
    const bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    const bool disabled = flags & JsonTreeNodeFlags_Disabled;
    const ImGuiTreeNodeFlags imgui_flags = flags & JsonTreeNodeFlags_DefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

    if (disabled) ImGui::BeginDisabled();
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Text, s.Style.FlowGrid.Colors[FlowGridCol_HighlightText]);
    const bool is_open = id ? ImGui::TreeNodeEx(id, imgui_flags, "%s", label.c_str()) : ImGui::TreeNodeEx(label.c_str(), imgui_flags);
    if (highlighted) ImGui::PopStyleColor();
    if (disabled) ImGui::EndDisabled();

    return is_open;
}

void fg::JsonTree(const string &label, const json &value, JsonTreeNodeFlags node_flags, const char *id) {
    if (value.is_null()) {
        ImGui::Text("%s", label.empty() ? "(null)" : label.c_str());
    } else if (value.is_object()) {
        if (label.empty() || JsonTreeNode(label, node_flags, id)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                JsonTree(it.key(), it.value(), node_flags);
            }
            if (!label.empty()) ImGui::TreePop();
        }
    } else if (value.is_array()) {
        if (label.empty() || JsonTreeNode(label, node_flags, id)) {
            int i = 0;
            for (const auto &it: value) {
                JsonTree(std::to_string(i), it, node_flags);
                i++;
            }
            if (!label.empty()) ImGui::TreePop();
        }
    } else {
        if (label.empty()) ImGui::Text("%s", value.dump().c_str());
        else ImGui::Text("%s: %s", label.c_str(), value.dump().c_str());
    }
}
