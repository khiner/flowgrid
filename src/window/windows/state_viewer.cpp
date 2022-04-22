#include "../windows.h"
#include "../../context.h"
#include "../../imgui_helpers.h"

using Settings = WindowsBase::StateViewerWindow::Settings;

bool HighlightedTreeNode(const char *label, bool is_highlighted = false) {
    if (is_highlighted) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255)); // TODO register a highlight color in style
    bool is_open = ImGui::TreeNode(label);
    if (is_highlighted) ImGui::PopStyleColor();

    return is_open;
}

static void add_json_state_value_node(const std::string &key, const json &value, bool is_annotated_key = false) {
    bool annotate = s.ui.windows.state_viewer.settings.label_mode == Settings::annotated;
    //      ImGuiTreeNodeFlags_DefaultOpen or SetNextItemOpen()
    if (value.is_null()) {
        ImGui::Text("null");
    } else if (value.is_object()) {
        if (HighlightedTreeNode(key.c_str(), is_annotated_key)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                add_json_state_value_node(it.key(), it.value());
            }
            ImGui::TreePop();
        }
    } else if (value.is_array()) {
        bool annotate_color = annotate && key == "Colors";
        if (HighlightedTreeNode(key.c_str(), is_annotated_key)) {
            int i = 0;
            for (const auto &it: value) {
                const bool is_child_annotated_key = annotate_color && i < ImGuiCol_COUNT;
                const auto &name = is_child_annotated_key ? ImGui::GetStyleColorName(i) : std::to_string(i);
                add_json_state_value_node(name, it, is_child_annotated_key);
                i++;
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("%s : %s", key.c_str(), value.dump().c_str());
    }
}

static const std::string label_help = "The raw JSON state doesn't store keys for all items.\n"
                                      "For example, the main `ui.style.colors` state is a list.\n\n"
                                      "'Annotated' mode shows (highlighted) labels for such state items.\n"
                                      "'Raw' mode shows the state exactly as it is in the raw JSON state.";

void StateViewer::draw(Window &) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Settings")) {
            if (BeginMenuWithHelp("Label mode", label_help.c_str())) {
                auto label_mode = s.ui.windows.state_viewer.settings.label_mode;
                if (ImGui::MenuItem("Annotated", nullptr, label_mode == Settings::annotated)) {
                    q.enqueue(set_state_viewer_label_mode{Settings::LabelMode::annotated});
                } else if (ImGui::MenuItem("Raw", nullptr, label_mode == Settings::raw)) {
                    q.enqueue(set_state_viewer_label_mode{Settings::LabelMode::raw});
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    add_json_state_value_node("State", c.json_state);
}
