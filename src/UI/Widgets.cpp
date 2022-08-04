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

bool fg::BeginMenuWithHelp(const char *label, const char *help, bool enabled) {
    HelpMarker(help);
    ImGui::SameLine();
    return ImGui::BeginMenu(label, enabled);
}

bool fg::MenuItemWithHelp(const char *label, const char *help, const char *shortcut, bool selected, bool enabled) {
    HelpMarker(help);
    ImGui::SameLine();
    return ImGui::MenuItem(label, shortcut, selected, enabled);
}

void gestured() {
    if (ImGui::IsItemActivated()) c.is_widget_gesturing = true;
    if (ImGui::IsItemDeactivated()) c.is_widget_gesturing = false;
}

void fg::Checkbox(const JsonPath &path, const char *label) {
    bool v = sj[path];
    if (ImGui::Checkbox(label ? label : path_label(path).c_str(), &v)) q(toggle_value{path});
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

bool fg::SliderInt(const char *label, int *v, int v_min, int v_max, const char *format, ImGuiSliderFlags flags) {
    const bool edited = ImGui::SliderInt(label, v, v_min, v_max, format, flags);
    gestured();
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

bool fg::Combo(const JsonPath &path, const std::vector<int> &options) {
    int v = sj[path];
    auto it = std::find(options.begin(), options.end(), v);
    if (it == options.end()) throw std::runtime_error("Value " + std::to_string(v) + " not found in options");

    auto index = int(it - options.begin());
    auto items = options | transform([](int option) { return std::to_string(option); }) | views::join('\0') | to<string>;
    const bool edited = ImGui::Combo(path_label(path).c_str(), &index, items.c_str());
    if (edited) q(set_value{path, options[index]});
    return edited;
}

bool fg::JsonTreeNode(const string &label, JsonTreeNodeFlags flags, const char *id) {
    const bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    const bool disabled = flags & JsonTreeNodeFlags_Disabled;

    if (disabled) ImGui::BeginDisabled();
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Text, s.style.flowgrid.Colors[FlowGridCol_HighlightText]);
    const bool is_open = id ? ImGui::TreeNodeEx(id, flags, "%s", label.c_str()) : ImGui::TreeNode(label.c_str());
    if (highlighted) ImGui::PopStyleColor();
    if (disabled) ImGui::EndDisabled();

    return is_open;
}

void fg::JsonTree(const string &label, const json &value, const char *id) {
    if (value.is_null()) {
        ImGui::Text("%s", label.c_str());
    } else if (value.is_object()) {
        if (JsonTreeNode(label, JsonTreeNodeFlags_None, id)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                JsonTree(it.key(), it.value());
            }
            ImGui::TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(label, JsonTreeNodeFlags_None, id)) {
            int i = 0;
            for (const auto &it: value) {
                JsonTree(std::to_string(i), it);
                i++;
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("%s : %s", label.c_str(), value.dump().c_str());
    }
}
