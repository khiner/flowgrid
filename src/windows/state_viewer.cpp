#include "../context.h"
#include "../imgui_helpers.h"

#include "imgui_memory_editor.h"

using LabelMode = Windows::StateWindows::StateViewer::LabelMode;

typedef int JsonTreeNodeFlags;
enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None,
    JsonTreeNodeFlags_Highlighted,
    JsonTreeNodeFlags_Disabled,
};

bool JsonTreeNode(const char *label, JsonTreeNodeFlags flags) {
    bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    bool disabled = flags & JsonTreeNodeFlags_Disabled;

    if (disabled) ImGui::BeginDisabled();
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Text, state.style.flowgrid.Colors[FlowGridCol_HighlightText]);
    bool is_open = ImGui::TreeNode(label);
    if (highlighted) ImGui::PopStyleColor();
    if (disabled) ImGui::EndDisabled();

    return is_open;
}

bool is_number(const std::string &str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

enum ColorPaths_ {
    ColorPaths_ImGui,
    ColorPaths_ImPlot,
    ColorPaths_FlowGrid,
};
//using ColorPaths = int;

static const std::filesystem::path color_paths[] = { // addressable by `ColorPaths`
    "/style/imgui/Colors",
    "/style/implot/Colors",
    "/style/flowgrid/Colors",
};

static void show_json_state_value_node(const std::string &key, const json &value, const std::filesystem::path &path) {
    const bool auto_select = s.windows.state.viewer.auto_select;
    const bool annotate_enabled = s.windows.state.viewer.label_mode == LabelMode::annotated;

    const std::string &file_name = path.filename();
    const bool is_array_item = is_number(file_name);
    const int array_index = is_array_item ? std::stoi(file_name) : -1;
    const bool is_color = std::string(path).find("Colors") != std::string::npos && is_array_item;
    const bool is_imgui_color = path.parent_path() == color_paths[ColorPaths_ImGui];
    const bool is_implot_color = path.parent_path() == color_paths[ColorPaths_ImPlot];
    const bool is_flowgrid_color = path.parent_path() == color_paths[ColorPaths_FlowGrid];
    const auto &name = annotate_enabled ?
                       (is_imgui_color ?
                        ImGui::GetStyleColorName(array_index) : is_implot_color ? ImPlot::GetStyleColorName(array_index) :
                                                                is_flowgrid_color ? FlowGridStyle::GetColorName(array_index) :
                                                                is_array_item ? file_name : key) : key;

    // TODO ImGui::SetNextItemOpen(if me or any descendant was recently updated);
    //  or ImGuiTreeNodeFlags_DefaultOpen ?
    if (auto_select) {
        const auto &update_paths = c.state_stats.most_recent_update_paths;
        const auto is_ancestor_path = [path](const std::string &candidate_path) { return candidate_path.rfind(path, 0) == 0; };
        const bool was_recently_updated = std::find_if(update_paths.begin(), update_paths.end(), is_ancestor_path) != update_paths.end();
        ImGui::SetNextItemOpen(was_recently_updated);
    }

    JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None;
    if (annotate_enabled && is_color) node_flags |= JsonTreeNodeFlags_Highlighted;
    if (auto_select) node_flags |= JsonTreeNodeFlags_Disabled;

    // TODO update to new behavior of add/remove ops affecting _parent_ json path.
    if (value.is_null()) {
        ImGui::Text("null");
    } else if (value.is_object()) {
        if (JsonTreeNode(name.c_str(), node_flags)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                show_json_state_value_node(it.key(), it.value(), path / it.key());
            }
            ImGui::TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(name.c_str(), node_flags)) {
            int i = 0;
            for (const auto &it: value) {
                show_json_state_value_node(std::to_string(i), it, path / std::to_string(i));
                i++;
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::Text("%s : %s", name.c_str(), value.dump().c_str());

        if (c.state_stats.update_times_for_state_path.contains(path)) {
            const ImVec2 w_min = ImGui::GetWindowPos();
            const float w_width = ImGui::GetWindowWidth();
            const ImVec2 w_max = {w_min.x + w_width, w_min.y + ImGui::GetWindowHeight()};
            const ImVec2 item_min = ImGui::GetItemRectMin();
            const ImVec2 item_max = ImGui::GetItemRectMax();
            const ImVec2 row_min = {w_min.x, item_min.y};
            const ImVec2 row_max = {w_max.x, item_max.y};
            const float row_width = w_width;

            const auto &update_times = c.state_stats.update_times_for_state_path.at(path);
            const float max_ratio = float(update_times.size()) / float(c.state_stats.max_num_updates);

            static const auto flash_duration_ns = Nanos(500ms).count(); // TODO move to state (new type on `state.style`)

            // Flash background color on update
            const SystemTime now = time_point_cast<Nanos>(Clock::now());
            const SystemTime most_recent_update_time = update_times.back();
            const auto flash_remaining_ns = now.time_since_epoch().count() - most_recent_update_time.time_since_epoch().count();
            const float flash_complete_ratio = float(flash_remaining_ns) / float(flash_duration_ns);

            // Acts like a tree-histogram, where length of the line corresponds to relative update frequency
            // with `width => most frequently updated`.
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                row_min, {row_min.x + row_width * max_ratio, row_max.y},
                ImColor(ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram)),
                0.0f, ImDrawFlags_None
            );

            auto flash_color = state.style.flowgrid.Colors[FlowGridCol_Flash];
            flash_color.w = std::max(0.0f, 1 - flash_complete_ratio);
            ImGui::GetBackgroundDrawList()->AddRectFilled(row_min, row_max, ImColor(flash_color), 0.0f, ImDrawFlags_None);

            // TODO indicate relative update-recency
        }
    }
}

void Windows::StateWindows::MemoryEditorWindow::draw() {
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

void Windows::StateWindows::StatePathUpdateFrequency::draw() {
    if (c.state_stats.update_times_for_state_path.empty()) {
        ImGui::Text("No state updates yet.");
        return;
    }

    auto &[labels, values] = c.state_stats.path_update_frequency_plottable;

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30.0f + 60.0f}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        if (labels.size() == 1) labels.emplace_back("");

        ImPlot::PushStyleColor(ImPlotCol_Fill, ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram));
        ImPlot::SetupAxisTicks(ImAxis_X1, 0, double(c.state_stats.max_num_updates), int(c.state_stats.max_num_updates) + 1, nullptr, false);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        ImPlot::PlotBarsH("Number of updates", values.data(), int(values.size()), 0.75, 0);
        ImPlot::EndPlot();
        ImPlot::PopStyleColor();
    }
}

static const std::string label_help = "The raw JSON state doesn't store keys for all items.\n"
                                      "For example, the main `ui.style.colors` state is a list.\n\n"
                                      "'Annotated' mode shows (highlighted) labels for such state items.\n"
                                      "'Raw' mode shows the state exactly as it is in the raw JSON state.";
static const std::string auto_select_help = "When auto-select is enabled, state changes automatically open.\n"
                                            "The state viewer to the changed state node(s), closing all other state nodes.\n"
                                            "State menu items can only be opened or closed manually if auto-select is disabled.";

void Windows::StateWindows::StateViewer::draw() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Settings")) {
            if (MenuItemWithHelp("Auto-select", auto_select_help.c_str(), nullptr, s.windows.state.viewer.auto_select)) {
                q(toggle_state_viewer_auto_select{});
            }
            if (BeginMenuWithHelp("Label mode", label_help.c_str())) {
                auto label_mode = s.windows.state.viewer.label_mode;
                if (ImGui::MenuItem("Annotated", nullptr, label_mode == LabelMode::annotated)) {
                    q(set_state_viewer_label_mode{LabelMode::annotated});
                } else if (ImGui::MenuItem("Raw", nullptr, label_mode == LabelMode::raw)) {
                    q(set_state_viewer_label_mode{LabelMode::raw});
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    show_json_state_value_node("State", c.state_json, "/");
}
