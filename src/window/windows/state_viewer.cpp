#include "../windows.h"
#include "../../context.h"
#include "../../imgui_helpers.h"

#include "implot.h"
#include "imgui_memory_editor.h"

using Settings = WindowsData::StateWindows::StateViewerWindow::Settings;
using LabelMode = Settings::LabelMode;

bool HighlightedTreeNode(const char *label, bool is_highlighted = false) {
    if (is_highlighted) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255)); // TODO register a highlight color in style
    bool is_open = ImGui::TreeNode(label);
    if (is_highlighted) ImGui::PopStyleColor();

    return is_open;
}

static void show_json_state_value_node(const std::string &key, const json &value, bool is_annotated_key = false) {
    bool annotate = s.ui.windows.state.viewer.settings.label_mode == LabelMode::annotated;
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

void StateWindows::MemoryEditorWindow::draw() {
    static MemoryEditor memory_editor;
    static bool first_render{true};
    if (first_render) {
        memory_editor.OptShowDataPreview = true;
        first_render = false;
    }

    void *mem_data{&ui_s};
    size_t mem_size{sizeof(ui_s)};
    memory_editor.DrawContents(mem_data, mem_size);
}

void StateWindows::StatePathUpdateFrequency::draw() {
    if (c.state_stats.update_times_for_state_path.empty()) {
        ImGui::Text("No state updates yet.");
        return;
    }

    auto &[labels, values] = c.state_stats.path_update_frequency_plottable;
    const ImU64 max_value = *std::max_element(values.begin(), values.end());

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30.0f + 60.0f}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        if (labels.size() == 1) labels.emplace_back("");

        ImPlot::SetupAxisTicks(ImAxis_X1, 0, double(max_value), int(max_value) + 1, nullptr, false);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        ImPlot::PlotBarsH("Number of updates", values.data(), int(values.size()), 0.75, 0);
        ImPlot::EndPlot();
    }
}

static const std::string label_help = "The raw JSON state doesn't store keys for all items.\n"
                                      "For example, the main `ui.style.colors` state is a list.\n\n"
                                      "'Annotated' mode shows (highlighted) labels for such state items.\n"
                                      "'Raw' mode shows the state exactly as it is in the raw JSON state.";

void StateWindows::StateViewer::draw() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Settings")) {
            if (BeginMenuWithHelp("Label mode", label_help.c_str())) {
                auto label_mode = s.ui.windows.state.viewer.settings.label_mode;
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
}
