#include "../windows.h"
#include "../../context.h"
#include "../../imgui_helpers.h"

#include "implot.h"

using Settings = WindowsBase::StateViewerWindow::Settings;
using LabelMode = Settings::LabelMode;

bool HighlightedTreeNode(const char *label, bool is_highlighted = false) {
    if (is_highlighted) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255)); // TODO register a highlight color in style
    bool is_open = ImGui::TreeNode(label);
    if (is_highlighted) ImGui::PopStyleColor();

    return is_open;
}

static void show_json_state_value_node(const std::string &key, const json &value, bool is_annotated_key = false) {
    bool annotate = s.ui.windows.state_viewer.settings.label_mode == LabelMode::annotated;
    //      ImGuiTreeNodeFlags_DefaultOpen or SetNextItemOpen()
    if (value.is_null()) {
        ImGui::Text("null");
    } else if (value.is_object()) {
        if (HighlightedTreeNode(key.c_str(), is_annotated_key)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                show_json_state_value_node(it.key(), it.value());
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
                show_json_state_value_node(name, it, is_child_annotated_key);
                i++;
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("%s : %s", key.c_str(), value.dump().c_str());
    }
}

static void show_path_update_frequency() {
    if (c.state_stats.action_times_for_state_path.empty()) return;

    if (ImPlot::BeginPlot("Path update frequency", ImVec2(-1, 400), ImPlotFlags_NoMouseText)) {
        static const char *keys[] = {"Number of updates"};
        const auto &[labels, values] = c.state_stats.path_update_frequency_plottable;

        ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);
//        ImPlot::SetupAxisTicks(ImAxis_X1, 0, int(labels.size() - 1), int(labels.size()), labels.data(), false);
        if (labels.size() > 1) ImPlot::SetupAxisTicks(ImAxis_Y1, 0, int(labels.size() - 1), int(labels.size()), labels.data(), false);
        ImPlot::PlotBarGroupsH(keys, values.data(), 1, int(values.size()), 0.75, 0);
        ImPlot::EndPlot();
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
                if (ImGui::MenuItem("Annotated", nullptr, label_mode == LabelMode::annotated)) {
                    q.enqueue(set_state_viewer_label_mode{LabelMode::annotated});
                } else if (ImGui::MenuItem("Raw", nullptr, label_mode == LabelMode::raw)) {
                    q.enqueue(set_state_viewer_label_mode{LabelMode::raw});
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    show_json_state_value_node("State", c.json_state);
    show_path_update_frequency();
}
