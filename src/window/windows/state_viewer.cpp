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
        ImGui::Text("%s : %s", key.c_str(), value.dump().c_str());
    }
}

void StateViewer::draw(Window &) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::BeginMenu("Label mode")) {
                auto label_mode = ui_s.ui.windows.state_viewer.settings.label_mode;
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

    add_json_value_node("State", c.json_state);
}
