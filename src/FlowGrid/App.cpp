#include "Store/Store.h"

#include "App.h"

#include <iostream>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"

#include "Helper/File.h"
#include "ProjectConstants.h"

using namespace nlohmann;

// todo should be in `Store`, but first need to separate out the core action stuff so `Store.h` can include `Action/Core.h` and define its own actions.
namespace store {
void Apply(const Action::StoreAction &action) {
    using namespace Action;
    Match(
        action,
        [](const SetValue &a) { Set(a.path, a.value); },
        [](const SetValues &a) { Set(a.values); },
        [](const SetVector &a) { Set(a.path, a.value); },
        [](const SetMatrix &a) { Set(a.path, a.data, a.row_count); },
        [](const ToggleValue &a) { Set(a.path, !std::get<bool>(store::Get(a.path))); },
        [](const Action::ApplyPatch &a) { ApplyPatch(a.patch); },
    );
}
} // namespace store

void State::Apply(const Action::StatefulAction &action) const {
    using namespace Action;
    Match(
        action,
        [](const Action::StoreAction &a) { store::Apply(a); },
        [&](const Action::FileDialogAction &a) { FileDialog.Apply(a); },
        [&](const Action::StyleAction &a) {
            Match(
                a,
                // todo enum types instead of raw integers
                [&](const SetImGuiColorStyle &a) {
                    switch (a.id) {
                        case 0: return Style.ImGui.ColorsDark();
                        case 1: return Style.ImGui.ColorsLight();
                        case 2: return Style.ImGui.ColorsClassic();
                    }
                },
                [&](const SetImPlotColorStyle &a) {
                    switch (a.id) {
                        case 0: return Style.ImPlot.ColorsAuto();
                        case 1: return Style.ImPlot.ColorsDark();
                        case 2: return Style.ImPlot.ColorsLight();
                        case 3: return Style.ImPlot.ColorsClassic();
                    }
                },
                [&](const SetFlowGridColorStyle &a) {
                    switch (a.id) {
                        case 0: return Style.FlowGrid.ColorsDark();
                        case 1: return Style.FlowGrid.ColorsLight();
                        case 2: return Style.FlowGrid.ColorsClassic();
                    }
                },
                [&](const SetGraphColorStyle &a) {
                    switch (a.id) {
                        case 0: return Audio.Faust.Graph.Style.ColorsDark();
                        case 1: return Audio.Faust.Graph.Style.ColorsLight();
                        case 2: return Audio.Faust.Graph.Style.ColorsClassic();
                        case 3: return Audio.Faust.Graph.Style.ColorsFaust();
                    }
                },
                [&](const SetGraphLayoutStyle &a) {
                    switch (a.id) {
                        case 0: return Audio.Faust.Graph.Style.LayoutFlowGrid();
                        case 1: return Audio.Faust.Graph.Style.LayoutFaust();
                    }
                },
            );
        },
        [&](const ShowOpenProjectDialog &) { FileDialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", ""}); },
        [&](const ShowSaveProjectDialog &) { FileDialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
        [&](const ShowOpenFaustFileDialog &) { FileDialog.Set({"Choose file", FaustDspFileExtension, ".", ""}); },
        [&](const ShowSaveFaustFileDialog &) { FileDialog.Set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1}); },
        [&](const ShowSaveFaustSvgFileDialog &) { FileDialog.Set({"Choose directory", ".*", ".", "faust_graph", true, 1}); },

        [&](const OpenFaustFile &a) { store::Set(Audio.Faust.Code, FileIO::read(a.path)); },
        [&](const CloseApplication &) { store::Set({{UiProcess.Running, false}, {Audio.Device.On, false}}); },
    );
}

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

using namespace ImGui;

void State::UIProcess::Render() const {}

void State::Render() const {
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
        ApplicationSettings.Dock(settings_node_id);

        Audio.Faust.Editor.Dock(faust_editor_node_id);
        Audio.Faust.Editor.Metrics.Dock(dockspace_id); // What's remaining of the main dockspace after splitting is used for the editor metrics.
        Audio.Faust.Log.Dock(faust_tools_node_id);
        Audio.Faust.Graph.Dock(faust_tools_node_id);
        Audio.Faust.Params.Dock(faust_tools_node_id);

        Debug.StateViewer.Dock(debug_node_id);
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
        Debug.StateViewer.SelectTab(); // not visible by default anymore
    }

    // Draw non-window children.
    for (const auto *child : Children) {
        if (const auto *ui_child = dynamic_cast<const UIStateMember *>(child)) {
            if (!dynamic_cast<const Window *>(child)) {
                ui_child->Draw();
            }
        }
    }
    // Recursively draw all windows.
    DrawWindows();
}

//-----------------------------------------------------------------------------
// [SECTION] State windows
//-----------------------------------------------------------------------------

#include "Helper/String.h"
#include "Store/StoreHistory.h"
#include "UI/Widgets.h"

ImRect RowItemRatioRect(float ratio) {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    return {row_min, row_min + ImVec2{GetWindowWidth() * std::clamp(ratio, 0.f, 1.f), GetFontSize()}};
}

using namespace FlowGrid;

void FillRowItemBg(const U32 col = style.ImGui.Colors[ImGuiCol_FrameBgActive]) {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    const ImRect &rect = {row_min, row_min + ImVec2{GetWindowWidth(), GetFontSize()}};
    GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, col);
}

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

#include "Store/StoreJson.h"
#include "date.h"

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

void Style::FlowGridStyle::Render() const {
    using namespace Action;

    static int colors_idx = -1, graph_colors_idx = -1, graph_layout_idx = -1;
    if (Combo("Colors", &colors_idx, "Dark\0Light\0Classic\0")) q(SetFlowGridColorStyle{colors_idx});
    if (Combo("Graph colors", &graph_colors_idx, "Dark\0Light\0Classic\0Faust\0")) q(SetGraphColorStyle{graph_colors_idx});
    if (Combo("Graph layout", &graph_layout_idx, "FlowGrid\0Faust\0")) q(SetGraphLayoutStyle{graph_layout_idx});
    FlashDurationSec.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem("Matrix mixer", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Graph.Style.Matrix.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust graph", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.Graph.Style.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.Params.Style.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Colors.ImGuiLabel.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
            Colors.Draw();
        }
        EndTabBar();
    }
}

#include "AppPreferences.h"

void OpenRecentProject::MenuItem() const {
    if (BeginMenu("Open recent project", !Preferences.RecentlyOpenedPaths.empty())) {
        for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
            if (ImGui::MenuItem(recently_opened_path.filename().c_str())) q(Action::OpenProject{recently_opened_path});
        }
        EndMenu();
    }
}

void ApplicationSettings::Render() const {
    int value = int(History.Index);
    if (SliderInt("History index", &value, 0, int(History.Size() - 1))) q(Action::SetHistoryIndex{value});
    GestureDurationSec.Draw();
}

Demo::Demo(StateMember *parent, string_view path_segment, string_view name_help)
    : TabsWindow(parent, path_segment, name_help, ImGuiWindowFlags_MenuBar) {}

void Demo::ImGuiDemo::Render() const {
    ShowDemoWindow();
}
void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}

#include "PrimitiveJson.h"

void Metrics::FlowGridMetrics::Render() const {
    {
        // Active (uncompressed) gesture
        const bool widget_gesturing = UiContext.IsWidgetGesturing;
        const bool ActiveGesturePresent = !History.ActiveGesture.empty();
        if (ActiveGesturePresent || widget_gesturing) {
            // Gesture completion progress bar
            const auto row_item_ratio_rect = RowItemRatioRect(1 - History.GestureTimeRemainingSec(s.ApplicationSettings.GestureDurationSec) / s.ApplicationSettings.GestureDurationSec);
            GetWindowDrawList()->AddRectFilled(row_item_ratio_rect.Min, row_item_ratio_rect.Max, style.FlowGrid.Colors[FlowGridCol_GestureIndicator]);

            const auto &ActiveGesture_title = "Active gesture"s + (ActiveGesturePresent ? " (uncompressed)" : "");
            if (TreeNodeEx(ActiveGesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (widget_gesturing) FillRowItemBg();
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
                        JsonTree("", store_record);
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

#include "Audio/Faust/FaustGraph.h"

void Info::Render() const {
    const auto hovered_id = GetHoveredID();
    if (!hovered_id) return;

    PushTextWrapPos(0);
    if (StateMember::WithId.contains(hovered_id)) {
        const auto *member = StateMember::WithId.at(hovered_id);
        const string help = member->Help.empty() ? std::format("No info available for \"{}\".", member->Name) : member->Help;
        TextUnformatted(help.c_str());
    } else if (IsBoxHovered(hovered_id)) {
        TextUnformatted(GetBoxInfo(hovered_id).c_str());
    }
    PopTextWrapPos();
}

//-----------------------------------------------------------------------------
// [SECTION] Project
//-----------------------------------------------------------------------------

struct GesturesProject {
    const Action::Gestures gestures;
    const Count index;
};

GesturesProject JsonToGestures(const nlohmann::json &j) {
    return {j["gestures"], j["index"]};
}

nlohmann::json GetStoreJson(const StoreJsonFormat format) {
    switch (format) {
        case StateFormat: return AppStore;
        case ActionFormat: return {{"gestures", History.Gestures()}, {"index", History.Index}};
    }
}

static std::optional<fs::path> CurrentProjectPath;
static bool ProjectHasChanges{false};

void SetCurrentProjectPath(const fs::path &path) {
    ProjectHasChanges = false;
    CurrentProjectPath = path;
    Preferences.OnProjectOpened(path);
}

static bool IsUserProjectPath(const fs::path &path) {
    return fs::relative(path).string() != fs::relative(EmptyProjectPath).string() &&
        fs::relative(path).string() != fs::relative(DefaultProjectPath).string();
}

std::optional<StoreJsonFormat> GetStoreJsonFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (StoreJsonFormatForExtension.contains(ext)) return StoreJsonFormatForExtension.at(ext);
    return {};
}

bool SaveProject(const fs::path &path) {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !Action::SaveCurrentProject::Allowed()) return false;

    const auto format = GetStoreJsonFormat(path);
    if (!format) return false; // TODO log

    History.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, GetStoreJson(*format).dump())) return false; // TODO log

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
    return true;
}

void Project::SaveEmptyProject() { SaveProject(EmptyProjectPath); }

// Main setter to modify the canonical application state store.
// _All_ store assignments happen via this method.
Patch SetStore(const Store &store) {
    const auto &patch = store::CreatePatch(AppStore, store);
    if (patch.Empty()) return {};

    store::Set(store);
    History.LatestUpdatedPaths = patch.Ops | views::transform([&patch](const auto &entry) { return patch.BasePath / entry.first; }) | to<vector>;
    ProjectHasChanges = true;

    static std::set<Base *> modified_fields;
    modified_fields.clear();
    for (const auto &path : History.LatestUpdatedPaths) {
        // Find all updated fields, including container fields.
        auto modified_field = Base::WithPath.find(path);
        if (modified_field == Base::WithPath.end()) modified_field = Base::WithPath.find(path.parent_path());
        if (modified_field == Base::WithPath.end()) modified_field = Base::WithPath.find(path.parent_path().parent_path());
        if (modified_field == Base::WithPath.end()) throw std::runtime_error(std::format("`SetStore` resulted in a patch affecting a path belonging to an unknown field: {}", path.string()));

        modified_fields.emplace(modified_field->second);

        // Setting `ImGuiSettings` does not require a `s.Apply` on the action, since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        if (path.string().rfind(imgui_settings.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiSettings; // TODO only when not ui-initiated
        else if (path.string().rfind(fg::style.ImGui.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiStyle;
        else if (path.string().rfind(fg::style.ImPlot.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImPlotStyle;
    }
    for (auto *modified_field : modified_fields) modified_field->Update();

    return patch;
}

void SetHistoryIndex(Count index) {
    History.SetIndex(index);
    SetStore(History.CurrentStore());
}

void Project::Init() {
    store::EndTransient(); // Make sure the store is not in transient mode when initializing a project.
    CurrentProjectPath = {};
    ProjectHasChanges = false;
    History = {AppStore};
    UiContext.IsWidgetGesturing = false;
}

#include "UI/Widgets.h"
#include "imgui.h"

using namespace ImGui;

void Debug::StateViewer::Render() const {
    StateJsonTree("State", GetStoreJson(StateFormat));
}

void Debug::ProjectPreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    const nlohmann::json project_json = GetStoreJson(StoreJsonFormat(int(Format)));
    if (Raw) TextUnformatted(project_json.dump(4).c_str());
    else fg::JsonTree("", project_json, JsonTreeNodeFlags_DefaultOpen);
}

void OpenProject(const fs::path &path) {
    const auto format = GetStoreJsonFormat(path);
    if (!format) return; // TODO log

    Project::Init();

    const nlohmann::json project = nlohmann::json::parse(FileIO::read(path));
    if (format == StateFormat) {
        SetStore(JsonToStore(project));
    } else if (format == ActionFormat) {
        OpenProject(EmptyProjectPath);

        const auto &[gestures, index] = JsonToGestures(project);
        store::BeginTransient();
        for (const auto &gesture : gestures) {
            const auto before_store = store::GetPersistent();
            for (const auto &action_moment : gesture) {
                s.Apply(action_moment.first);
            }
            const auto after_store = store::GetPersistent();
            const auto &patch = store::CreatePatch(before_store, after_store);
            const auto &gesture_time = gesture.back().second;
            History.Add(gesture_time, after_store, gesture); // todo save/load gesture commit times
            for (const auto &[partial_path, op] : patch.Ops) {
                History.CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
            }
        }
        SetStore(store::EndTransient(false));
        ::SetHistoryIndex(index);
    }

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
}

//-----------------------------------------------------------------------------
// [SECTION] Action queueing
//-----------------------------------------------------------------------------

namespace Action {
bool OpenDefaultProject::Allowed() { return fs::exists(DefaultProjectPath); }
bool ShowSaveProjectDialog::Allowed() { return ProjectHasChanges; }
bool SaveCurrentProject::Allowed() { return ProjectHasChanges; }
} // namespace Action

#include "blockingconcurrentqueue.h"

using Action::ActionMoment, Action::StatefulActionMoment;
inline static moodycamel::BlockingConcurrentQueue<ActionMoment> ActionQueue;

void Apply(const Action::NonStatefulAction &action) {
    Match(
        action,
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const Action::OpenEmptyProject &) { OpenProject(EmptyProjectPath); },
        [&](const Action::OpenProject &a) { OpenProject(a.path); },
        [&](const Action::OpenDefaultProject &) { OpenProject(DefaultProjectPath); },

        [&](const Action::SaveProject &a) { SaveProject(a.path); },
        [&](const Action::SaveDefaultProject &) { SaveProject(DefaultProjectPath); },
        [&](const Action::SaveCurrentProject &) {
            if (CurrentProjectPath) SaveProject(*CurrentProjectPath);
        },
        [&](const Action::SaveFaustFile &a) { FileIO::write(a.path, audio.Faust.Code); },
        [](const Action::SaveFaustSvgFile &a) { SaveBoxSvg(a.path); },

        // `History.Index`-changing actions:
        [&](const Action::Undo &) {
            if (History.Empty()) return;

            // `StoreHistory::SetIndex` reverts the current gesture before applying the new history index.
            // If we're at the end of the stack, we want to finalize the active gesture and add it to the stack.
            // Otherwise, if we're already in the middle of the stack somewhere, we don't want an active gesture
            // to finalize and cut off everything after the current history index, so an undo just ditches the active changes.
            // (This allows consistent behavior when e.g. being in the middle of a change and selecting a point in the undo history.)
            if (History.Index == History.Size() - 1) {
                if (!History.ActiveGesture.empty()) History.FinalizeGesture();
                ::SetHistoryIndex(History.Index - 1);
            } else {
                ::SetHistoryIndex(History.Index - (History.ActiveGesture.empty() ? 1 : 0));
            }
        },
        [&](const Action::Redo &) { SetHistoryIndex(History.Index + 1); },
        [&](const Action::SetHistoryIndex &a) { SetHistoryIndex(a.index); },
    );
}

void Project::RunQueuedActions(bool force_finalize_gesture) {
    static ActionMoment action_moment;
    static vector<StatefulActionMoment> state_actions; // Same type as `Gesture`, but doesn't represent a full semantic "gesture".
    state_actions.clear();

    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, _] = action_moment;
        if (!action.IsAllowed()) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can tell us where to save it:
        if (std::holds_alternative<Action::SaveCurrentProject>(action) && !CurrentProjectPath) action = Action::ShowSaveProjectDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_finalize_gesture |= std::holds_alternative<Action::ToggleValue>(action);

        Match(
            action,
            [&](const Action::StatefulAction &a) {
                store::BeginTransient(); // Idempotent.
                s.Apply(a);
                state_actions.emplace_back(a, action_moment.second);
            },
            // Note: `const auto &` capture does not work when the other type is itself a variant group. Need to be exhaustive.
            [&](const Action::NonStatefulAction &a) {
                // todo really we want to separate out stateful and non-stateful actions, and commit each batch of stateful actions.
                if (store::IsTransientMode()) throw std::runtime_error("Non-stateful action in the same batch as stateful action (in transient mode).");
                Apply(a);
            },
        );
    }

    const bool finalize = force_finalize_gesture || (!UiContext.IsWidgetGesturing && !History.ActiveGesture.empty() && History.GestureTimeRemainingSec(s.ApplicationSettings.GestureDurationSec) <= 0);
    if (!state_actions.empty()) {
        const auto &patch = SetStore(store::EndTransient(false));
        History.ActiveGesture.insert(History.ActiveGesture.end(), state_actions.begin(), state_actions.end());
        History.UpdateGesturePaths(state_actions, patch);
    } else {
        store::EndTransient(false);
    }
    if (finalize) History.FinalizeGesture();
}

bool q(const Action::Any &&action, bool flush) {
    ActionQueue.enqueue({action, Clock::now()});
    if (flush) Project::RunQueuedActions(true); // If the `flush` flag is set, we finalize the gesture now.
    return true;
}
