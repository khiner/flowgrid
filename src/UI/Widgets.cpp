#include "Widgets.h"
#include "../Context.h"

void fg::HelpMarker(const char *desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void gestured() {
    if (ImGui::IsItemActivated()) c.is_widget_gesturing = true;
    if (ImGui::IsItemDeactivated()) c.is_widget_gesturing = false;
}

bool fg::Checkbox(const JsonPath &path, const char *label) {
    bool v = sj[path];
    bool edited = ImGui::Checkbox(label ? label : path_label(path).c_str(), &v);
    if (edited) q(toggle_value{path});
    return edited;
}

bool fg::SliderFloat(const JsonPath &path, float v_min, float v_max, const char *format, ImGuiSliderFlags flags, const char *label) {
    float v = sj[path];
    const bool edited = ImGui::SliderFloat(label ? label : path_label(path).c_str(), &v, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::SliderFloat2(const JsonPath &path, float v_min, float v_max, const char *format, ImGuiSliderFlags flags) {
    ImVec2 v = sj[path];
    const bool edited = ImGui::SliderFloat2(path_label(path).c_str(), (float *) &v, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::SliderInt(const JsonPath &path, int v_min, int v_max, const char *format, ImGuiSliderFlags flags, const char *label) {
    int v = sj[path];
    const bool edited = ImGui::SliderInt(label ? label : path_label(path).c_str(), &v, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::DragFloat(const JsonPath &path, float v_speed, float v_min, float v_max, const char *format, ImGuiSliderFlags flags, const char *label) {
    float v = sj[path];
    const bool edited = ImGui::DragFloat(label ? label : path_label(path).c_str(), &v, v_speed, v_min, v_max, format, flags);
    gestured();
    if (edited) q(set_value{path, v});
    return edited;
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

bool fg::Combo(const JsonPath &path, const char *items_separated_by_zeros, int popup_max_height_in_items) {
    int v = sj[path];
    const bool edited = ImGui::Combo(path_label(path).c_str(), &v, items_separated_by_zeros, popup_max_height_in_items);
    if (edited) q(set_value{path, v});
    return edited;
}

bool fg::JsonTreeNode(const string &label, JsonTreeNodeFlags flags, const char *id) {
    const bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    const bool disabled = flags & JsonTreeNodeFlags_Disabled;
    const ImGuiTreeNodeFlags imgui_flags = flags & JsonTreeNodeFlags_DefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

    if (disabled) ImGui::BeginDisabled();
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Text, s.style.flowgrid.Colors[FlowGridCol_HighlightText]);
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
