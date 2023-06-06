#include "Debug.h"

#include "date.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

#include "App/Style/Style.h"
#include "Core/Action/Actions.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"
#include "Helper/String.h"
#include "Project/ProjectJson.h"
#include "UI/Widgets.h"

using namespace ImGui;

void Debug::Render() const {}

void Debug::StorePathUpdateFrequency::Render() const {
    auto [labels, values] = History.StorePathUpdateFrequencyPlottable();
    if (labels.empty()) {
        Text("No state updates yet.");
        return;
    }

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30 + 60}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        // todo fix in ImPlot
        if (labels.size() == 1) labels.emplace_back("");

        // todo add an axis flag to exclude non-integer ticks
        // todo add an axis flag to show last tick
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        static const char *ItemLabels[] = {"Committed updates", "Active updates"};
        const int item_count = !History.ActiveGesture.empty() ? 2 : 1;
        const int group_count = int(values.size()) / item_count;
        ImPlot::PlotBarGroups(ItemLabels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

void Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void Debug::StackTool::Render() const {
    ShowStackToolWindow();
}

void Metrics::ImGuiMetrics::Render() const { ShowMetricsWindow(); }
void Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

using namespace FlowGrid;

// TODO option to indicate relative update-recency
void Debug::StateViewer::StateJsonTree(string_view key, const json &value, const StorePath &path) const {
    const string leaf_name = path == RootPath ? path.string() : path.filename().string();
    const auto &parent_path = path == RootPath ? path : path.parent_path();
    const bool is_array_item = StringHelper::IsInteger(leaf_name);
    const int array_index = is_array_item ? std::stoi(leaf_name) : -1;
    const bool is_imgui_color = parent_path == style.ImGui.Colors.Path;
    const bool is_implot_color = parent_path == style.ImPlot.Colors.Path;
    const bool is_flowgrid_color = parent_path == style.FlowGrid.Colors.Path;
    const string label = LabelMode == Annotated ?
        (is_imgui_color        ? style.ImGui.Colors.Child(array_index)->Name :
             is_implot_color   ? style.ImPlot.Colors.Child(array_index)->Name :
             is_flowgrid_color ? style.FlowGrid.Colors.Child(array_index)->Name :
             is_array_item     ? leaf_name :
                                 string(key)) :
        string(key);

    if (AutoSelect) {
        const auto &updated_paths = History.LatestUpdatedPaths;
        const auto is_ancestor_path = [&path](const string &candidate_path) { return candidate_path.rfind(path.string(), 0) == 0; };
        const bool was_recently_updated = std::find_if(updated_paths.begin(), updated_paths.end(), is_ancestor_path) != updated_paths.end();
        SetNextItemOpen(was_recently_updated);
    }

    // Flash background color of nodes when its corresponding path updates.
    const auto &latest_update_time = History.LatestUpdateTime(path);
    if (latest_update_time) {
        const float flash_elapsed_ratio = fsec(Clock::now() - *latest_update_time).count() / style.FlowGrid.FlashDurationSec;
        ImColor flash_color = style.FlowGrid.Colors[FlowGridCol_GestureIndicator];
        flash_color.Value.w = std::max(0.f, 1 - flash_elapsed_ratio);
        FillRowItemBg(flash_color);
    }

    JsonTreeNodeFlags flags = JsonTreeNodeFlags_None;
    if (LabelMode == Annotated && (is_imgui_color || is_implot_color || is_flowgrid_color)) flags |= JsonTreeNodeFlags_Highlighted;
    if (AutoSelect) flags |= JsonTreeNodeFlags_Disabled;

    // The rest below is structurally identical to `fg::Widgets::JsonTree`.
    // Couldn't find an easy/clean way to inject the above into each recursive call.
    if (value.is_null()) {
        TextUnformatted(label.c_str());
    } else if (value.is_object()) {
        if (JsonTreeNode(label, flags)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                StateJsonTree(it.key(), *it, path / it.key());
            }
            TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(label, flags)) {
            Count i = 0;
            for (const auto &it : value) {
                StateJsonTree(to_string(i), it, path / to_string(i));
                i++;
            }
            TreePop();
        }
    } else {
        JsonTreeNode(label, flags, nullptr, value.dump().c_str());
    }
}

void Debug::StateViewer::Render() const {
    StateJsonTree("State", GetProjectJson(StateFormat));
}

void Debug::ProjectPreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    const nlohmann::json project_json = GetProjectJson(ProjectJsonFormat(int(Format)));
    if (Raw) TextUnformatted(project_json.dump(4).c_str());
    else fg::JsonTree("", project_json, JsonTreeNodeFlags_DefaultOpen);
}

void ShowGesture(const Gesture &gesture) {
    for (Count action_index = 0; action_index < gesture.size(); action_index++) {
        const auto &[action, time] = gesture[action_index];
        JsonTree(
            std::format("{}: {}", action.GetName(), date::format("%Y-%m-%d %T", time).c_str()),
            json(action)[1],
            JsonTreeNodeFlags_None,
            to_string(action_index).c_str()
        );
    }
}

#include "AppPreferences.h"
#include "Settings.h"

void Metrics::FlowGridMetrics::Render() const {
    {
        // Active (uncompressed) gesture
        const bool widget_gesturing = Stateful::Field::IsGesturing;
        const bool ActiveGesturePresent = !History.ActiveGesture.empty();
        if (ActiveGesturePresent || widget_gesturing) {
            // Gesture completion progress bar
            const float gesture_duration_sec = application_settings.GestureDurationSec;
            const auto row_item_ratio_rect = RowItemRatioRect(1 - History.GestureTimeRemainingSec(gesture_duration_sec) / gesture_duration_sec);
            GetWindowDrawList()->AddRectFilled(row_item_ratio_rect.Min, row_item_ratio_rect.Max, style.FlowGrid.Colors[FlowGridCol_GestureIndicator]);

            const auto &ActiveGesture_title = "Active gesture"s + (ActiveGesturePresent ? " (uncompressed)" : "");
            if (TreeNodeEx(ActiveGesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (widget_gesturing) FillRowItemBg(style.ImGui.Colors[ImGuiCol_FrameBgActive]);
                else BeginDisabled();
                Text("Widget gesture: %s", widget_gesturing ? "true" : "false");
                if (!widget_gesturing) EndDisabled();

                if (ActiveGesturePresent) ShowGesture(History.ActiveGesture);
                else Text("No actions yet");
                TreePop();
            }
        } else {
            BeginDisabled();
            Text("No active gesture");
            EndDisabled();
        }
    }
    Separator();
    {
        const bool no_history = History.Empty();
        if (no_history) BeginDisabled();
        if (TreeNodeEx("StoreHistory", ImGuiTreeNodeFlags_DefaultOpen, "Store event records (Count: %d, Current index: %d)", History.Size() - 1, History.Index)) {
            for (Count i = 1; i < History.Size(); i++) {
                if (TreeNodeEx(to_string(i).c_str(), i == History.Index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &[committed, store_record, gesture] = History.RecordAt(i);
                    BulletText("Committed: %s\n", date::format("%Y-%m-%d %T", committed).c_str());
                    if (TreeNode("Patch")) {
                        // We compute patches as we need them rather than memoizing them.
                        const auto &patch = History.CreatePatch(i);
                        for (const auto &[partial_path, op] : patch.Ops) {
                            const auto &path = patch.BasePath / partial_path;
                            if (TreeNodeEx(path.string().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                                BulletText("Op: %s", to_string(op.Op).c_str());
                                if (op.Value) BulletText("Value: %s", to_string(*op.Value).c_str());
                                if (op.Old) BulletText("Old value: %s", to_string(*op.Old).c_str());
                                TreePop();
                            }
                        }
                        TreePop();
                    }
                    if (TreeNode("Gesture")) {
                        ShowGesture(gesture);
                        TreePop();
                    }
                    if (TreeNode("State")) {
                        JsonTree("", store::GetJson(store_record));
                        TreePop();
                    }
                    TreePop();
                }
            }
            TreePop();
        }
        if (no_history) EndDisabled();
    }
    Separator();
    {
        // Preferences
        const bool has_RecentlyOpenedPaths = !Preferences.RecentlyOpenedPaths.empty();
        if (TreeNodeEx("Preferences", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (SmallButton("Clear")) Preferences.Clear();
            SameLine();
            ShowRelativePaths.Draw();

            if (!has_RecentlyOpenedPaths) BeginDisabled();
            if (TreeNodeEx("Recently opened paths", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
                    BulletText("%s", (ShowRelativePaths ? fs::relative(recently_opened_path) : recently_opened_path).c_str());
                }
                TreePop();
            }
            if (!has_RecentlyOpenedPaths) EndDisabled();

            TreePop();
        }
    }
    Separator();
    {
        // Various internals
        Text("Action variant size: %lu bytes", sizeof(Action::StatefulAction));
        Text("Primitive variant size: %lu bytes", sizeof(Primitive));
        SameLine();
        HelpMarker("All actions are internally stored in a `std::variant`, which must be large enough to hold its largest type. "
                   "Thus, it's important to keep action data minimal.");
    }
}

// #include "imgui_memory_editor.h"

// todo need to rethink this with the store system
// void Debug::StateMemoryEditor::Render() const {
//     static MemoryEditor memory_editor;
//     static bool first_render{true};
//     if (first_render) {
//         memory_editor.OptShowDataPreview = true;
//         //        memory_editor.WriteFn = ...; todo write_state_bytes action
//         first_render = false;
//     }

//     const void *mem_data{&s};
//     memory_editor.DrawContents(mem_data, sizeof(s));
// }
