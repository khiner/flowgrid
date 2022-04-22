#include "../windows.h"
#include "../../context.h"
#include "../../imgui_helpers.h"

static void add_json_state_value_node(const std::string &key, const json &value, bool annotate = false) {
    //      ImGuiTreeNodeFlags_DefaultOpen or SetNextItemOpen()
    if (value.is_null()) {
        ImGui::Text("null");
    } else if (value.is_object()) {
        if (ImGui::TreeNode(key.c_str())) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                add_json_state_value_node(it.key(), it.value(), annotate);
            }
            ImGui::TreePop();
        }
    } else if (value.is_array()) {
        bool annotate_color = annotate && key == "Colors";
        if (ImGui::TreeNode(key.c_str())) {
            int i = 0;
            for (const auto &it: value) {
                const auto &name = annotate_color && i < ImGuiCol_COUNT ? ImGui::GetStyleColorName(i) : std::to_string(i);
                // TODO color annotated labels
                add_json_state_value_node(name, it, annotate);
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
                                      "'Annotated' mode shows labels for such state items.\n"
                                      "'Raw' mode shows the state exactly as it is in the raw JSON state.";

void StateViewer::draw(Window &) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Settings")) {
            if (BeginMenuWithHelp("Label mode", label_help.c_str())) {
                auto label_mode = s.ui.windows.state_viewer.settings.label_mode;
                if (ImGui::MenuItem("Annotated", nullptr, label_mode == WindowsBase::StateViewerWindow::Settings::annotated)) {
                    q.enqueue(set_state_viewer_label_mode{Windows::StateViewerWindow::Settings::LabelMode::annotated});
                } else if (ImGui::MenuItem("Raw", nullptr, label_mode == WindowsBase::StateViewerWindow::Settings::raw)) {
                    q.enqueue(set_state_viewer_label_mode{Windows::StateViewerWindow::Settings::LabelMode::raw});
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    bool annotate = s.ui.windows.state_viewer.settings.label_mode == WindowsBase::StateViewerWindow::Settings::annotated;
    add_json_state_value_node("State", c.json_state, annotate);
}
