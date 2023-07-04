
#include "Project.h"

#include "imgui_internal.h"
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <set>

#include "Application/ApplicationPreferences.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"
#include "Helper/File.h"
#include "Helper/Time.h"
#include "UI/UI.h"

using std::vector;
using namespace FlowGrid;

static SavableActionMoments ActiveGestureActions{}; // uncompressed, uncommitted
static Patch LatestPatch;

// Project constants:
static const fs::path InternalPath = ".flowgrid";

// Order matters here, as the first extension is the default project extension.
static const std::map<ProjectFormat, std::string> ExtensionByProjectFormat{
    {ProjectFormat::ActionFormat, ".fla"},
    {ProjectFormat::StateFormat, ".fls"},
};
static const auto ProjectFormatByExtension = ExtensionByProjectFormat | std::views::transform([](const auto &p) { return std::pair(p.second, p.first); }) | ranges::to<std::map>();
static const auto AllProjectExtensions = ProjectFormatByExtension | std::views::keys;
static const std::string AllProjectExtensionsDelimited = AllProjectExtensions | ranges::views::join(',') | ranges::to<std::string>;

static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionByProjectFormat.at(ProjectFormat::StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionByProjectFormat.at(ProjectFormat::ActionFormat));

static std::optional<fs::path> CurrentProjectPath;
static bool ProjectHasChanges{false};

std::optional<ProjectFormat> GetProjectFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (!ProjectFormatByExtension.contains(ext)) return {};
    return ProjectFormatByExtension.at(ext);
}

static float GestureTimeRemainingSec(float gesture_duration_sec) {
    if (ActiveGestureActions.empty()) return 0;
    const auto ret = std::max(0.f, gesture_duration_sec - fsec(Clock::now() - ActiveGestureActions.back().QueueTime).count());
    return ret;
}

Project::Project(ComponentArgs &&args) : Component(std::move(args)) {
    Windows.SetWindowComponents({
        Audio,
        Settings,
        Audio.Faust.Code,
        Audio.Faust.Code.Debug,
        Audio.Faust.Log,
        Audio.Faust.Graph,
        Audio.Faust.Params,
        Debug,
        Debug.ProjectPreview,
        Debug.StorePathUpdateFrequency,
        Debug.DebugLog,
        Debug.StackTool,
        Debug.Metrics,
        Style,
        Demo,
        Info,
    });
}

nlohmann::json Project::ToJson(const ProjectFormat format) const {
    switch (format) {
        case StateFormat: return store.GetJson();
        case ActionFormat: return History.GetIndexedGestures();
    }
}

void CommitGesture() {
    Field::GestureChangedPaths.clear();
    if (ActiveGestureActions.empty()) return;

    const auto merged_actions = MergeActions(ActiveGestureActions);
    ActiveGestureActions.clear();
    if (merged_actions.empty()) return;

    History.AddGesture({merged_actions, Clock::now()});
}

void SetHistoryIndex(Count index) {
    if (index == History.Index) return;

    Field::GestureChangedPaths.clear();
    // If we're mid-gesture, revert the current gesture before navigating to the new index.
    ActiveGestureActions.clear();
    History.SetIndex(index);
    LatestPatch = store.CheckedSet(History.CurrentStore());
    Field::RefreshChanged(LatestPatch);
    // ImGui settings are cheched separately from style since we don't need to re-apply ImGui settings state to ImGui context
    // when it initially changes, since ImGui has already updated its own context.
    // We only need to update the ImGui context based on settings changes when the history index changes.
    // However, style changes need to be applied to the ImGui context in all cases, since these are issued from Field changes.
    // We don't make `ImGuiSettings` a field change listener for this because it would would end up being slower,
    // since it has many descendent fields, and we would wastefully check for changes during the forward action pass, as explained above.
    if (LatestPatch.IsPrefixOfAnyPath(imgui_settings.Path)) imgui_settings.IsChanged = true;
    ProjectHasChanges = true;
}

void Project::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Project::OpenEmpty &) { Open(EmptyProjectPath); },
        [](const Action::Project::Open &a) { Open(a.file_path); },
        [](const Action::Project::OpenDefault &) { Open(DefaultProjectPath); },

        [this](const Action::Project::Save &a) { Save(a.file_path); },
        [this](const Action::Project::SaveDefault &) { Save(DefaultProjectPath); },
        [this](const Action::Project::SaveCurrent &) {
            if (CurrentProjectPath) Save(*CurrentProjectPath);
        },
        // History-changing actions:
        [](const Action::Project::Undo &) {
            if (History.Empty()) return;

            // `StoreHistory::SetIndex` reverts the current gesture before applying the new history index.
            // If we're at the end of the stack, we want to commit the active gesture and add it to the stack.
            // Otherwise, if we're already in the middle of the stack somewhere, we don't want an active gesture
            // to commit and cut off everything after the current history index, so an undo just ditches the active changes.
            // (This allows consistent behavior when e.g. being in the middle of a change and selecting a point in the undo history.)
            if (History.Index == History.Size() - 1) {
                if (!ActiveGestureActions.empty()) CommitGesture();
                ::SetHistoryIndex(History.Index - 1);
            } else {
                ::SetHistoryIndex(History.Index - (ActiveGestureActions.empty() ? 1 : 0));
            }
        },
        [](const Action::Project::Redo &) { SetHistoryIndex(History.Index + 1); },
        [](const Action::Project::SetHistoryIndex &a) { SetHistoryIndex(a.index); },

        [](const FieldActionHandler::ActionType &a) { Field::ActionHandler.Apply(a); },
        [](const Store::ActionType &a) { store.Apply(a); },
        [this](const Action::Project::ShowOpenDialog &) { FileDialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", ""}); },
        [this](const Action::Project::ShowSaveDialog &) { FileDialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
        [this](const Audio::ActionType &a) { Audio.Apply(a); },
        [this](const FileDialog::ActionType &a) { FileDialog.Apply(a); },
        [this](const Windows::ActionType &a) { Windows.Apply(a); },
        [this](const Style::ActionType &a) { Style.Apply(a); },
    );
}

bool Project::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [](const Action::Project::Undo &) { return !ActiveGestureActions.empty() || History.CanUndo(); },
        [](const Action::Project::Redo &) { return History.CanRedo(); },
        [](const Action::Project::Save &) { return !History.Empty(); },
        [](const Action::Project::SaveDefault &) { return !History.Empty(); },
        [](const Action::Project::ShowSaveDialog &) { return ProjectHasChanges; },
        [](const Action::Project::SaveCurrent &) { return ProjectHasChanges; },
        [](const Action::Project::OpenDefault &) { return fs::exists(DefaultProjectPath); },

        [](const FieldActionHandler::ActionType &a) { return Field::ActionHandler.CanApply(a); },
        [](const Store::ActionType &a) { return store.CanApply(a); },
        [this](const Audio::ActionType &a) { return Audio.CanApply(a); },
        [this](const FileDialog::ActionType &a) { return FileDialog.CanApply(a); },
        [this](const Windows::ActionType &a) { return Windows.CanApply(a); },
        [this](const Style::ActionType &a) { return Style.CanApply(a); },
        [](const auto &) { return true; },
    );
}

void Apply(const Action::Savable &action) {
    Visit(
        action,
        [](const Project::ActionType &a) { project.Apply(a); },
    );
}

using namespace ImGui;

void Project::Render() const {
    static const auto Shortcuts = Action::Any::CreateShortcuts();

    const auto &io = GetIO();
    for (const auto &[action_id, shortcut] : Shortcuts) {
        const auto &[mod, key] = shortcut.Parsed;
        if (mod == io.KeyMods && IsKeyPressed(GetKeyIndex(ImGuiKey(key)))) {
            const auto action = Action::Any::Create(action_id);
            if (CanApply(action)) action.q();
        }
    }

    MainMenu.Draw();

    // Good initial layout setup example in this issue: https://github.com/ocornut/imgui/issues/3548
    auto dockspace_id = DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    int frame_count = GetCurrentContext()->FrameCount;
    if (frame_count == 1) {
        auto settings_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        auto utilities_node_id = DockBuilderSplitNode(settings_node_id, ImGuiDir_Down, 0.5f, nullptr, &settings_node_id);

        auto debug_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
        auto metrics_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Right, 0.35f, nullptr, &debug_node_id);

        auto info_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);
        auto faust_tools_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.5f, nullptr, &dockspace_id);
        auto faust_editor_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.7f, nullptr, &dockspace_id);

        Audio.Dock(settings_node_id);
        Settings.Dock(settings_node_id);

        Audio.Faust.Code.Dock(faust_editor_node_id);
        Audio.Faust.Code.Debug.Dock(dockspace_id); // What's remaining of the main dockspace after splitting is used for the editor metrics.
        Audio.Faust.Log.Dock(faust_tools_node_id);
        Audio.Faust.Graph.Dock(faust_tools_node_id);
        Audio.Faust.Params.Dock(faust_tools_node_id);

        Debug.Dock(debug_node_id);
        Debug.ProjectPreview.Dock(debug_node_id);
        // Debug.StateMemoryEditor.Dock(debug_node_id);
        Debug.StorePathUpdateFrequency.Dock(debug_node_id);
        Debug.DebugLog.Dock(debug_node_id);
        Debug.StackTool.Dock(debug_node_id);
        Debug.Metrics.Dock(metrics_node_id);

        Style.Dock(utilities_node_id);
        Demo.Dock(utilities_node_id);

        Info.Dock(info_node_id);
    } else if (frame_count == 2) {
        // Doesn't work on the first draw: https://github.com/ocornut/imgui/issues/2304
        Style.SelectTab();
        Audio.SelectTab();
        Audio.Faust.Graph.SelectTab();
        Debug.SelectTab(); // not visible by default anymore
    }

    // Draw non-window children.
    for (const auto *child : Children) {
        if (child == &Windows) continue;
        if (!Windows.IsWindow(child->Id)) child->Draw();
    }
    Windows.Draw();

    static string PrevSelectedPath = "";
    if (PrevSelectedPath != FileDialog.SelectedFilePath) {
        const fs::path selected_path = FileDialog.SelectedFilePath;
        const string &extension = selected_path.extension();
        if (std::ranges::find(AllProjectExtensions, extension) != AllProjectExtensions.end()) {
            if (FileDialog.SaveMode) Action::Project::Save{selected_path}.q();
            else Action::Project::Open{selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }
}

void Project::OpenRecentProjectMenuItem() {
    if (BeginMenu("Open recent project", !Preferences.RecentlyOpenedPaths.empty())) {
        for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
            if (ImGui::MenuItem(recently_opened_path.filename().c_str())) Action::Project::Open{recently_opened_path}.q();
        }
        EndMenu();
    }
}

bool IsUserProjectPath(const fs::path &path) {
    return fs::relative(path).string() != fs::relative(EmptyProjectPath).string() &&
        fs::relative(path).string() != fs::relative(DefaultProjectPath).string();
}

void SetCurrentProjectPath(const fs::path &path) {
    ProjectHasChanges = false;
    if (IsUserProjectPath(path)) {
        CurrentProjectPath = path;
        Preferences.OnProjectOpened(path);
    } else {
        CurrentProjectPath = {};
    }
}

bool Project::Save(const fs::path &path) const {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !ProjectHasChanges) return false;

    const auto format = GetProjectFormat(path);
    if (!format) return false; // TODO log

    CommitGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, ToJson(*format).dump())) {
        throw std::runtime_error(std::format("Failed to write project file: {}", path.string()));
    }

    SetCurrentProjectPath(path);
    return true;
}

// When loading a new project, we always refresh all UI contexts.
void MarkAllUiContextsChanged() {
    style.ImGui.IsChanged = true;
    style.ImPlot.IsChanged = true;
    imgui_settings.IsChanged = true;
}

void Project::OnApplicationLaunch() const {
    Field::IsGesturing = false;
    History.Clear();
    Field::ClearChanged();
    Field::LatestChangedPaths.clear();
    MarkAllUiContextsChanged();

    // Keep the canonical "empty" project up-to-date.
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);
    Save(EmptyProjectPath);
}

nlohmann::json ReadFileJson(const fs::path &file_path) {
    return nlohmann::json::parse(FileIO::read(file_path));
}

// Helper function used in `Project::Open`.
void OpenStateFormatProjectInner(const nlohmann::json &project) {
    const auto &patch = store.SetJson(project);
    Field::RefreshChanged(patch);
    Field::ClearChanged();
    Field::LatestChangedPaths.clear();
    // Always update the ImGui context, regardless of the patch, to avoid expensive sifting through paths and just to be safe.
    imgui_settings.IsChanged = true;
    History.Clear();
}

void Project::Open(const fs::path &file_path) {
    const auto format = GetProjectFormat(file_path);
    if (!format) return; // TODO log

    Field::IsGesturing = false;

    const nlohmann::json project = ReadFileJson(file_path);
    if (format == StateFormat) {
        OpenStateFormatProjectInner(project);
    } else if (format == ActionFormat) {
        OpenStateFormatProjectInner(ReadFileJson(EmptyProjectPath));

        StoreHistory::IndexedGestures indexed_gestures = project;
        store.BeginTransient();
        for (auto &gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture.Actions) ::Apply(action_moment.Action);
            History.AddGesture(std::move(gesture));
        }
        LatestPatch = store.CheckedCommit();
        Field::RefreshChanged(LatestPatch);
        ::SetHistoryIndex(indexed_gestures.Index);
        Field::LatestChangedPaths.clear();
    }

    SetCurrentProjectPath(file_path);
}

//-----------------------------------------------------------------------------
// [SECTION] Debug
//-----------------------------------------------------------------------------

#include "date.h"
#include "implot.h"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/map.hpp>

#include "Helper/String.h"
#include "UI/HelpMarker.h"
#include "UI/JsonTree.h"

struct Plottable {
    std::vector<std::string> Labels;
    std::vector<ImU64> Values;
};

Plottable StorePathChangeFrequencyPlottable() {
    if (History.GetChangedPathsCount() == 0 && Field::GestureChangedPaths.empty()) return {};

    std::map<StorePath, Count> gesture_change_counts;
    for (const auto &[field_id, changed_paths] : Field::GestureChangedPaths) {
        const auto &field = Field::ById[field_id];
        for (const PathsMoment &paths_moment : changed_paths) {
            for (const auto &path : paths_moment.second) {
                gesture_change_counts[path == "" ? field->Path : field->Path / path]++;
            }
        }
    }

    const auto history_change_counts = History.GetChangeCountByPath();
    const std::set<StorePath> paths = ranges::views::concat(ranges::views::keys(history_change_counts), ranges::views::keys(gesture_change_counts)) | ranges::to<std::set>;

    Count i = 0;
    std::vector<ImU64> values(!gesture_change_counts.empty() ? paths.size() * 2 : paths.size());
    for (const auto &path : paths) {
        values[i++] = history_change_counts.contains(path) ? history_change_counts.at(path) : 0;
    }
    if (!gesture_change_counts.empty()) {
        // Optionally add a second plot item for gesturing update times.
        // See `ImPlot::PlotBarGroups` for value ordering explanation.
        for (const auto &path : paths) {
            values[i++] = gesture_change_counts.contains(path) ? gesture_change_counts.at(path) : 0;
        }
    }

    // Remove leading '/' from paths to create labels.
    return {
        paths | std::views::transform([](const string &path) { return path.substr(1); }) | ranges::to<std::vector>,
        values,
    };
}

void Project::Debug::StorePathUpdateFrequency::Render() const {
    auto [labels, values] = StorePathChangeFrequencyPlottable();
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
        const auto c_labels = labels | std::views::transform([](const std::string &label) { return label.c_str(); }) | ranges::to<std::vector>;
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), c_labels.data(), false);

        static const char *ItemLabels[] = {"Committed updates", "Active updates"};
        const int item_count = !ActiveGestureActions.empty() ? 2 : 1;
        const int group_count = values.size() / item_count;
        ImPlot::PlotBarGroups(ItemLabels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

void Project::Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void Project::Debug::StackTool::Render() const {
    ShowStackToolWindow();
}

void Project::Debug::Metrics::ImGuiMetrics::Render() const { ImGui::ShowMetricsWindow(); }
void Project::Debug::Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

using namespace FlowGrid;

void Project::RenderDebug() const {
    const bool auto_select = Debug.AutoSelect;
    if (auto_select) BeginDisabled();
    RenderValueTree(Debug::LabelModeType(int(Debug.LabelMode)) == Debug::LabelModeType::Annotated, auto_select);
    if (auto_select) EndDisabled();
}

void Project::Debug::ProjectPreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    const nlohmann::json project_json = project.ToJson(ProjectFormat(int(Format)));
    if (Raw) {
        TextUnformatted(project_json.dump(4).c_str());
    } else {
        SetNextItemOpen(true);
        fg::JsonTree("", project_json);
    }
}

void ShowActions(const SavableActionMoments &actions) {
    for (Count action_index = 0; action_index < actions.size(); action_index++) {
        const auto &[action, queue_time] = actions[action_index];
        if (TreeNodeEx(to_string(action_index).c_str(), ImGuiTreeNodeFlags_None, "%s", action.GetPath().string().c_str())) {
            BulletText("Queue time: %s", date::format("%Y-%m-%d %T", queue_time).c_str());
            SameLine();
            fg::HelpMarker("The original queue time of the action. If this is a merged action, this is the queue time of the most recent action in the merge.");
            const nlohmann::json data = nlohmann::json(action)[1];
            if (!data.is_null()) {
                SetNextItemOpen(true);
                JsonTree("Data", data);
            }
            TreePop();
        }
    }
}

ImRect RowItemRatioRect(float ratio) {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    return {row_min, row_min + ImVec2{GetWindowWidth() * std::clamp(ratio, 0.f, 1.f), GetFontSize()}};
}

void Project::Debug::Metrics::FlowGridMetrics::Render() const {
    {
        // Active (uncompressed) gesture
        const bool is_gesturing = Field::IsGesturing;
        const bool any_gesture_actions = !ActiveGestureActions.empty();
        if (any_gesture_actions || is_gesturing) {
            // Gesture completion progress bar (full-width to empty).
            const float gesture_duration_sec = project_settings.GestureDurationSec;
            const float time_remaining_sec = GestureTimeRemainingSec(gesture_duration_sec);
            const auto row_item_ratio_rect = RowItemRatioRect(time_remaining_sec / gesture_duration_sec);
            GetWindowDrawList()->AddRectFilled(row_item_ratio_rect.Min, row_item_ratio_rect.Max, style.FlowGrid.Colors[FlowGridCol_GestureIndicator]);

            const auto &ActiveGestureActions_title = "Active gesture"s + (any_gesture_actions ? " (uncompressed)" : "");
            if (TreeNodeEx(ActiveGestureActions_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (is_gesturing) FillRowItemBg(style.ImGui.Colors[ImGuiCol_FrameBgActive]);
                else BeginDisabled();
                Text("Widget gesture: %s", is_gesturing ? "true" : "false");
                if (!is_gesturing) EndDisabled();

                if (any_gesture_actions) ShowActions(ActiveGestureActions);
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
        if (TreeNodeEx("History", ImGuiTreeNodeFlags_DefaultOpen, "History (Records: %d, Current record index: %d)", History.Size() - 1, History.Index)) {
            for (Count i = 1; i < History.Size(); i++) {
                if (TreeNodeEx(to_string(i).c_str(), i == History.Index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &[store_record, gesture] = History.RecordAt(i);
                    BulletText("Gesture committed: %s\n", date::format("%Y-%m-%d %T", gesture.CommitTime).c_str());
                    if (TreeNode("Actions")) {
                        ShowActions(gesture.Actions);
                        TreePop();
                    }
                    if (TreeNode("Patch")) {
                        // We compute patches as we need them rather than memoizing.
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
                    if (TreeNode("State snapshot")) {
                        JsonTree("", store.GetJson(store_record));
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
        Text("Action variant size: %lu bytes", sizeof(Action::Savable));
        Text("Primitive variant size: %lu bytes", sizeof(Primitive));
        SameLine();
        fg::HelpMarker(
            "All actions are internally stored in a `std::variant`, which must be large enough to hold its largest type. "
            "Thus, it's important to keep action data minimal."
        );
    }
}

void Project::Debug::Metrics::Render() const {
    RenderTabs();
}

// #include "imgui_memory_editor.h"

// todo need to rethink this with the store system
// void Project::Debug::StateMemoryEditor::Render() const {
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

//-----------------------------------------------------------------------------
// [SECTION] Action queueing
//-----------------------------------------------------------------------------

#include "blockingconcurrentqueue.h"

inline static moodycamel::BlockingConcurrentQueue<ActionMoment> ActionQueue;

void q(const Action::Any &&action) {
    ActionQueue.enqueue({action, Clock::now()});
}

void RunQueuedActions(bool force_commit_gesture) {
    static ActionMoment action_moment;
    static vector<SavableActionMoment> stateful_actions; // Same type as `Gesture`, but doesn't represent a full semantic "gesture".
    stateful_actions.clear();

    if (file_dialog.Visible) {
        // Disable all actions while the file dialog is open.
        while (ActionQueue.try_dequeue(action_moment)) {};
        return;
    }

    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, queue_time] = action_moment;
        if (!project.CanApply(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can choose the save file:
        if (std::holds_alternative<Action::Project::SaveCurrent>(action) && !CurrentProjectPath) action = Action::Project::ShowSaveDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_commit_gesture |=
            std::holds_alternative<Action::Primitive::Bool::Toggle>(action) ||
            std::holds_alternative<Action::AdjacencyList::ToggleConnection>(action) ||
            std::holds_alternative<Action::FileDialog::Select>(action);

        const bool is_savable = action.IsSavable();
        if (is_savable) store.BeginTransient(); // Idempotent.
        // todo really we want to separate out stateful and non-stateful actions, and commit each batch of stateful actions.
        else if (!stateful_actions.empty()) throw std::runtime_error("Non-stateful action in the same batch as stateful action (in transient mode).");

        project.Apply(action);

        Visit(
            action,
            [&queue_time](const Action::Savable &a) { stateful_actions.emplace_back(a, queue_time); },
            // Note: `const auto &` capture does not work when the other type is itself a variant group. Need to be exhaustive.
            [](const Action::NonSavable &) {},
        );
    }

    const bool commit_gesture = force_commit_gesture ||
        (!Field::IsGesturing && !ActiveGestureActions.empty() && GestureTimeRemainingSec(project_settings.GestureDurationSec) <= 0);

    if (!stateful_actions.empty()) {
        LatestPatch = store.CheckedCommit();
        if (!LatestPatch.Empty()) {
            Field::RefreshChanged(LatestPatch, true);
            ActiveGestureActions.insert(ActiveGestureActions.end(), stateful_actions.begin(), stateful_actions.end());

            ProjectHasChanges = true;
        }
    } else {
        store.Commit(); // This ends transient mode but should not modify the state, since there were no stateful actions.
    }

    if (commit_gesture) CommitGesture();
}

#define DefineQ(ActionType)                                                                                                  \
    void Action::ActionType::q() const { ::q(*this); }                                                                       \
    void Action::ActionType::MenuItem() {                                                                                    \
        if (ImGui::MenuItem(GetMenuLabel().c_str(), GetShortcut().c_str(), false, project.CanApply(Action::ActionType{}))) { \
            Action::ActionType{}.q();                                                                                        \
        }                                                                                                                    \
    }

DefineQ(Windows::ToggleVisible);
DefineQ(Project::Undo);
DefineQ(Project::Redo);
DefineQ(Project::SetHistoryIndex);
DefineQ(Project::Open);
DefineQ(Project::OpenEmpty);
DefineQ(Project::OpenDefault);
DefineQ(Project::Save);
DefineQ(Project::SaveDefault);
DefineQ(Project::SaveCurrent);
DefineQ(Project::ShowOpenDialog);
DefineQ(Project::ShowSaveDialog);
DefineQ(Primitive::Bool::Toggle);
DefineQ(Primitive::Int::Set);
DefineQ(Primitive::UInt::Set);
DefineQ(Primitive::Float::Set);
DefineQ(Primitive::String::Set);
DefineQ(Primitive::Enum::Set);
DefineQ(Primitive::Flags::Set);
DefineQ(MultilineString::Set);
DefineQ(Vector<bool>::SetAt);
DefineQ(Vector<int>::SetAt);
DefineQ(Vector<U32>::SetAt);
DefineQ(Vector<float>::SetAt);
DefineQ(Vector2D<bool>::Set);
DefineQ(Vector2D<int>::Set);
DefineQ(Vector2D<U32>::Set);
DefineQ(Vector2D<float>::Set);
DefineQ(Vec2::Set);
DefineQ(Vec2::SetX);
DefineQ(Vec2::SetY);
DefineQ(Vec2::SetAll);
DefineQ(AdjacencyList::ToggleConnection);
DefineQ(Store::ApplyPatch);
DefineQ(Style::SetImGuiColorPreset);
DefineQ(Style::SetImPlotColorPreset);
DefineQ(Style::SetFlowGridColorPreset);
DefineQ(FaustFile::ShowOpenDialog);
DefineQ(FaustFile::ShowSaveDialog);
DefineQ(FaustFile::Save);
DefineQ(FaustFile::Open);
DefineQ(FaustGraph::SetColorStyle);
DefineQ(FaustGraph::SetLayoutStyle);
DefineQ(FaustGraph::ShowSaveSvgDialog);
DefineQ(FaustGraph::SaveSvgFile);
DefineQ(FileDialog::Open);
DefineQ(FileDialog::Select);
