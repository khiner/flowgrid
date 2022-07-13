#include "../Context.h"
#include "Menu.h"
#include "StatefulImGui.h"

#include "imgui_memory_editor.h"

using namespace ImGui;

using LabelMode = State::StateWindows::StateViewer::LabelMode;

typedef int JsonTreeNodeFlags;
enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
};

bool JsonTreeNode(const char *label, JsonTreeNodeFlags flags) {
    bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    bool disabled = flags & JsonTreeNodeFlags_Disabled;

    if (disabled) BeginDisabled();
    if (highlighted) PushStyleColor(ImGuiCol_Text, state.style.flowgrid.Colors[FlowGridCol_HighlightText]);
    bool is_open = TreeNode(label);
    if (highlighted) PopStyleColor();
    if (disabled) EndDisabled();

    return is_open;
}

bool is_number(const string &str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

enum ColorPaths_ {
    ColorPaths_ImGui,
    ColorPaths_ImPlot,
    ColorPaths_FlowGrid,
};
//using ColorPaths = int;

static const fs::path color_paths[] = { // addressable by `ColorPaths`
    "/style/imgui/Colors",
    "/style/implot/Colors",
    "/style/flowgrid/Colors",
};

static void show_json_state_value_node(const string &key, const json &value, const fs::path &path) {
    const bool auto_select = s.state.viewer.auto_select;
    const bool annotate_enabled = s.state.viewer.label_mode == LabelMode::annotated;

    const string &file_name = path.filename();
    const bool is_array_item = is_number(file_name);
    const int array_index = is_array_item ? std::stoi(file_name) : -1;
    const bool is_color = string(path).find("Colors") != string::npos && is_array_item;
    const bool is_imgui_color = path.parent_path() == color_paths[ColorPaths_ImGui];
    const bool is_implot_color = path.parent_path() == color_paths[ColorPaths_ImPlot];
    const bool is_flowgrid_color = path.parent_path() == color_paths[ColorPaths_FlowGrid];
    const auto &name = annotate_enabled ?
                       (is_imgui_color ?
                        GetStyleColorName(array_index) : is_implot_color ? ImPlot::GetStyleColorName(array_index) :
                                                         is_flowgrid_color ? FlowGridStyle::GetColorName(array_index) :
                                                         is_array_item ? file_name : key) : key;

    if (auto_select) {
        const auto &update_paths = c.state_stats.most_recent_update_paths;
        const auto is_ancestor_path = [path](const string &candidate_path) { return candidate_path.rfind(path, 0) == 0; };
        const bool was_recently_updated = std::find_if(update_paths.begin(), update_paths.end(), is_ancestor_path) != update_paths.end();
        SetNextItemOpen(was_recently_updated);
    }

    JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None;
    if (annotate_enabled && is_color) node_flags |= JsonTreeNodeFlags_Highlighted;
    if (auto_select) node_flags |= JsonTreeNodeFlags_Disabled;

    // Tree acts like a histogram, where rect length corresponds to relative update frequency, with `width => most frequently updated`.
    // Background color of nodes flashes on update.
    if (c.state_stats.update_times_for_state_path.contains(path)) {
        const auto &update_times = c.state_stats.update_times_for_state_path.at(path);

        const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
        const float item_w = GetWindowWidth();
        const ImVec2 row_max = {row_min.x + item_w, row_min.y + GetFontSize()};
        const float max_ratio = float(update_times.size()) / float(c.state_stats.max_num_updates);
        GetWindowDrawList()->AddRectFilled(
            row_min, {row_min.x + item_w * max_ratio, row_max.y},
            ImColor(GetStyleColorVec4(ImGuiCol_PlotHistogram)),
            0.0f, ImDrawFlags_None
        );

        // Flash background on update
        const auto most_recent_update_time = update_times.back();
        const fsec flash_remaining_sec = Clock::now() - most_recent_update_time;
        const float flash_complete_ratio = flash_remaining_sec.count() / s.style.flowgrid.FlashDurationSec;
        auto flash_color = s.style.flowgrid.Colors[FlowGridCol_Flash];
        flash_color.w = std::max(0.0f, 1 - flash_complete_ratio);
        GetWindowDrawList()->AddRectFilled(row_min, row_max, ImColor(flash_color), 0.0f, ImDrawFlags_None);

        // TODO indicate relative update-recency
    }

    if (value.is_null()) {
        Text("null");
    } else if (value.is_object()) {
        if (JsonTreeNode(name.c_str(), node_flags)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                show_json_state_value_node(it.key(), it.value(), path / it.key());
            }
            TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(name.c_str(), node_flags)) {
            int i = 0;
            for (const auto &it: value) {
                show_json_state_value_node(std::to_string(i), it, path / std::to_string(i));
                i++;
            }
            TreePop();
        }
    } else {
        Text("%s : %s", name.c_str(), value.dump().c_str());
    }
}

void State::StateWindows::StateMemoryEditor::draw() const {
    static MemoryEditor memory_editor;
    static bool first_render{true};
    if (first_render) {
        memory_editor.OptShowDataPreview = true;
        first_render = false;
    }

    void *mem_data{&c._state};
    size_t mem_size{sizeof(c._state)};
    memory_editor.DrawContents(mem_data, mem_size);
}

void State::StateWindows::StatePathUpdateFrequency::draw() const {
    if (c.state_stats.update_times_for_state_path.empty()) {
        Text("No state updates yet.");
        return;
    }

    auto &[labels, values] = c.state_stats.path_update_frequency_plottable;

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30.0f + 60.0f}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        if (labels.size() == 1) labels.emplace_back("");

        ImPlot::PushStyleColor(ImPlotCol_Fill, GetStyleColorVec4(ImGuiCol_PlotHistogram));
        ImPlot::SetupAxisTicks(ImAxis_X1, 0, double(c.state_stats.max_num_updates), int(c.state_stats.max_num_updates) + 1, nullptr, false);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        ImPlot::PlotBars("Number of updates", values.data(), int(values.size()), 0.75, 0, ImPlotBarsFlags_Horizontal);
        ImPlot::EndPlot();
        ImPlot::PopStyleColor();
    }
}

static const string label_help = "The raw JSON state doesn't store keys for all items.\n"
                                 "For example, the main `ui.style.colors` state is a list.\n\n"
                                 "'Annotated' mode shows (highlighted) labels for such state items.\n"
                                 "'Raw' mode shows the state exactly as it is in the raw JSON state.";
static const string auto_select_help = "When auto-select is enabled, state changes automatically open.\n"
                                       "The state viewer to the changed state node(s), closing all other state nodes.\n"
                                       "State menu items can only be opened or closed manually if auto-select is disabled.";

void State::StateWindows::StateViewer::draw() const {
    if (BeginMenuBar()) {
        if (BeginMenu("Settings")) {
            if (MenuItemWithHelp("Auto-select", auto_select_help.c_str(), nullptr, s.state.viewer.auto_select)) {
                q(toggle_state_viewer_auto_select{});
            }
            if (BeginMenuWithHelp("Label mode", label_help.c_str())) {
                if (MenuItem("Annotated", nullptr, label_mode == LabelMode::annotated)) {
                    q(set_state_viewer_label_mode{LabelMode::annotated});
                } else if (MenuItem("Raw", nullptr, label_mode == LabelMode::raw)) {
                    q(set_state_viewer_label_mode{LabelMode::raw});
                }
                EndMenu();
            }
            EndMenu();
        }
        EndMenuBar();
    }

    show_json_state_value_node("State", c.state_json, "/");
}

void State::StateWindows::draw() const {
    StatefulImGui::DrawWindow(memory_editor, ImGuiWindowFlags_NoScrollbar);
    StatefulImGui::DrawWindow(viewer, ImGuiWindowFlags_MenuBar);
    StatefulImGui::DrawWindow(path_update_frequency, ImGuiWindowFlags_None);
}
