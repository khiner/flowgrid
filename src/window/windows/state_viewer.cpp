#include "../windows.h"
#include "../../context.h"
#include "../../imgui_helpers.h"

#include "implot.h"
#include "imgui_memory_editor.h"

using Settings = WindowsBase::StateWindows::StateViewerWindow::Settings;
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

void StateWindows::MemoryEditorWindow::draw(Window &) {
    static MemoryEditor memory_editor;

    void *mem_data{&ui_s};
    size_t mem_size{sizeof(ui_s)};
    size_t base_display_addr{0x0000};

    MemoryEditor::Sizes sizes;
    memory_editor.CalcSizes(sizes, mem_size, base_display_addr);
    ImGui::SetNextWindowSize(ImVec2(sizes.WindowWidth, sizes.WindowWidth * 0.60f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(sizes.WindowWidth, FLT_MAX));

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("context");
    }

    memory_editor.DrawContents(mem_data, mem_size, base_display_addr);
    if (memory_editor.ContentsWidthChanged) {
        memory_editor.CalcSizes(sizes, mem_size, base_display_addr);
        ImGui::SetWindowSize(ImVec2(sizes.WindowWidth, ImGui::GetWindowSize().y));
    }
}

void StateWindows::StatePathUpdateFrequency::draw(Window &) {
    if (c.state_stats.update_times_for_state_path.empty()) {
        ImGui::Text("No state updates yet.");
        return;
    }

    auto &[labels, values] = c.state_stats.path_update_frequency_plottable;
    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 20.0f + 100.0f}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        if (labels.size() == 1) labels.emplace_back("");

        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        ImPlot::PlotBarsH("Number of updates", values.data(), int(values.size()), 0.75, 0);
        ImPlot::EndPlot();
    }
}

static const std::string label_help = "The raw JSON state doesn't store keys for all items.\n"
                                      "For example, the main `ui.style.colors` state is a list.\n\n"
                                      "'Annotated' mode shows (highlighted) labels for such state items.\n"
                                      "'Raw' mode shows the state exactly as it is in the raw JSON state.";

void StateWindows::StateViewer::draw(Window &) {
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
