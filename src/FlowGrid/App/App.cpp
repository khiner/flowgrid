
#include "App.h"

#include "imgui_internal.h"
#include "immer/map.hpp"
#include <range/v3/core.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/transform.hpp>

#include "AppPreferences.h"
#include "Core/Action/Actions.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"
#include "Core/Store/StoreJson.h"
#include "Helper/File.h"
#include "Helper/String.h"
#include "ProjectConstants.h"
#include "UI/UI.h"

using namespace FlowGrid;

static std::optional<fs::path> CurrentProjectPath;
static bool ProjectHasChanges{false};

void App::Apply(const Action::AppAction &action) const {
    using namespace Action;
    Match(
        action,
        [](const StoreAction &a) { store::Apply(a); },
        [&](const FileDialogAction &a) { FileDialog.Apply(a); },
        [&](const StyleAction &a) { Style.Apply(a); },
        [&](const AudioAction &a) { Audio.Apply(a); },
    );
}

bool App::CanApply(const Action::AppAction &action) const {
    using namespace Action;
    return Match(
        action,
        [](const StoreAction &a) { return store::CanApply(a); },
        [&](const FileDialogAction &a) { return FileDialog.CanApply(a); },
        [&](const StyleAction &a) { return Style.CanApply(a); },
        [&](const AudioAction &a) { return Audio.CanApply(a); },
    );
}

bool CanApply(const Action::Any &action) {
    using namespace Action;
    return Match(
        action,
        [&](const AppAction &a) { return app.CanApply(a); },
        [&](const ProjectAction &a) {
            return Match(
                a,
                [&](const Undo &) { return History.CanUndo(); },
                [&](const Redo &) { return History.CanRedo(); },
                [&](const SaveProject &) { return !History.Empty(); },
                [&](const SaveDefaultProject &) { return !History.Empty(); },
                [&](const ShowSaveProjectDialog &) { return ProjectHasChanges; },
                [&](const OpenDefaultProject &) { return fs::exists(DefaultProjectPath); },
                [&](const SaveCurrentProject &) { return ProjectHasChanges; },
                [&](const auto &) { return true; },
            );
        },
    );
}

using KeyShortcut = std::pair<ImGuiModFlags, ImGuiKey>;

const std::map<string, ImGuiModFlags> ModKeys{
    {"shift", ImGuiModFlags_Shift},
    {"ctrl", ImGuiModFlags_Ctrl},
    {"alt", ImGuiModFlags_Alt},
    {"cmd", ImGuiModFlags_Super},
};

// Handles any number of mods, followed by a single non-mod character.
// Example: 'shift+cmd+s'
// **Case-sensitive. `shortcut` must be lowercase.**
static constexpr std::optional<KeyShortcut> ParseShortcut(const string &shortcut) {
    const vector<string> tokens = StringHelper::Split(shortcut, "+");
    if (tokens.empty()) return {};

    const string command = tokens.back();
    if (command.length() != 1) return {};

    const auto key = ImGuiKey(command[0] - 'a' + ImGuiKey_A);
    ImGuiModFlags mod_flags = ImGuiModFlags_None;
    for (const auto &token : ranges::views::reverse(tokens) | ranges::views::drop(1)) {
        mod_flags |= ModKeys.at(token);
    }

    return {{mod_flags, key}};
}

// Transform `map<ActionID, string>` to `map<KeyShortcut, ActionID>`
const auto KeyMap = Action::Any::IndexToShortcut | ranges::views::transform([](const auto &entry) {
                        const auto &[action_id, shortcut] = entry;
                        return std::pair(*ParseShortcut(shortcut), action_id);
                    }) |
    ranges::to<std::map>;

using namespace ImGui;

void App::Render() const {
    const auto &io = GetIO();
    for (const auto &[shortcut, action_id] : KeyMap) {
        const auto &[mod, key] = shortcut;
        if (mod == io.KeyMods && IsKeyPressed(GetKeyIndex(key))) {
            const auto action = Action::Any::Create(action_id);
            if (::CanApply(action)) action.q();
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
        if (const auto *ui_child = dynamic_cast<const UIStateful *>(child)) {
            if (!dynamic_cast<const Stateful::Window *>(child)) {
                ui_child->Draw();
            }
        }
    }
    // Recursively draw all windows.
    DrawWindows();

    static string PrevSelectedPath = "";
    if (PrevSelectedPath != FileDialog.SelectedFilePath) {
        const fs::path selected_path = string(FileDialog.SelectedFilePath);
        const string &extension = selected_path.extension();
        if (AllProjectExtensions.find(extension) != AllProjectExtensions.end()) {
            if (FileDialog.SaveMode) Action::SaveProject{selected_path}.q();
            else Action::OpenProject{selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }
}

void App::OpenRecentProjectMenuItem() {
    if (BeginMenu("Open recent project", !Preferences.RecentlyOpenedPaths.empty())) {
        for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
            if (ImGui::MenuItem(recently_opened_path.filename().c_str())) Action::OpenProject{recently_opened_path}.q();
        }
        EndMenu();
    }
}

bool IsUserProjectPath(const fs::path &path) {
    return fs::relative(path).string() != fs::relative(EmptyProjectPath).string() &&
        fs::relative(path).string() != fs::relative(DefaultProjectPath).string();
}

void SetCurrentProjectPath(const fs::path &path) {
    if (!IsUserProjectPath(path)) return;

    ProjectHasChanges = false;
    CurrentProjectPath = path;
    Preferences.OnProjectOpened(path);
}

std::optional<StoreJsonFormat> GetStoreJsonFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (!StoreJsonFormatForExtension.contains(ext)) return {};
    return StoreJsonFormatForExtension.at(ext);
}

bool SaveProject(const fs::path &path) {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !ProjectHasChanges) return false;

    const auto format = GetStoreJsonFormat(path);
    if (!format) return false; // TODO log

    History.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, GetStoreJson(*format).dump())) return false; // TODO log

    SetCurrentProjectPath(path);
    return true;
}

void Project::SaveEmptyProject() { SaveProject(EmptyProjectPath); }

void OpenProject(const fs::path &); // Defined below.

void OnPatch(const Patch &patch) {
    if (patch.Empty()) return;

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

        // Setting `ImGuiSettings` does not require an `app.Apply` on the action, since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        if (path.string().rfind(imgui_settings.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImGuiSettings; // TODO only when not ui-initiated
        else if (path.string().rfind(fg::style.ImGui.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImGuiStyle;
        else if (path.string().rfind(fg::style.ImPlot.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImPlotStyle;
    }
    for (auto *modified_field : modified_fields) modified_field->Update();
}

void SetHistoryIndex(Count index) {
    History.SetIndex(index);
    OnPatch(store::CheckedSet(History.CurrentStore()));
}

void Project::Init() {
    store::Commit(); // Make sure the store is not in transient mode when initializing a project.
    CurrentProjectPath = {};
    ProjectHasChanges = false;
    History = {};
    Stateful::Field::IsGesturing = false;
}

void Apply(const Action::ProjectAction &action) {
    Match(
        action,
        [&](const Action::ShowOpenProjectDialog &) { file_dialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", ""}); },
        [&](const Action::ShowSaveProjectDialog &) { file_dialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
        [&](const Action::OpenEmptyProject &) { OpenProject(EmptyProjectPath); },
        [&](const Action::OpenProject &a) { OpenProject(a.path); },
        [&](const Action::OpenDefaultProject &) { OpenProject(DefaultProjectPath); },

        [&](const Action::SaveProject &a) { SaveProject(a.path); },
        [&](const Action::SaveDefaultProject &) { SaveProject(DefaultProjectPath); },
        [&](const Action::SaveCurrentProject &) {
            if (CurrentProjectPath) SaveProject(*CurrentProjectPath);
        },
        // History-changing actions:
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

void Apply(const Action::StatefulAction &action) {
    Match(
        action,
        [&](const Action::AppAction &a) { app.Apply(a); },
        [&](const Action::ProjectAction &a) { Apply(a); },
    );
}

void OpenProject(const fs::path &path) {
    const auto format = GetStoreJsonFormat(path);
    if (!format) return; // TODO log

    Project::Init();

    const nlohmann::json project = nlohmann::json::parse(FileIO::read(path));
    if (format == StateFormat) {
        OnPatch(store::CheckedSet(JsonToStore(project)));
        History = {};
    } else if (format == ActionFormat) {
        OpenProject(EmptyProjectPath);

        const StoreHistory::IndexedGestures indexed_gestures = project;
        store::BeginTransient();
        for (const auto &gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture) Apply(action_moment.first);
            History.Add(gesture.back().second, store::GetPersistent(), gesture); // todo save/load gesture commit times
        }
        OnPatch(store::CheckedCommit());
        ::SetHistoryIndex(indexed_gestures.Index);
    }

    SetCurrentProjectPath(path);
}

//-----------------------------------------------------------------------------
// [SECTION] Action queueing
//-----------------------------------------------------------------------------

#include "blockingconcurrentqueue.h"

using Action::ActionMoment, Action::StatefulActionMoment;
inline static moodycamel::BlockingConcurrentQueue<ActionMoment> ActionQueue;

void Project::RunQueuedActions(bool force_finalize_gesture) {
    static ActionMoment action_moment;
    static vector<StatefulActionMoment> stateful_actions; // Same type as `Gesture`, but doesn't represent a full semantic "gesture".
    stateful_actions.clear();

    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, _] = action_moment;
        if (!CanApply(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can tell us where to save it:
        if (std::holds_alternative<Action::SaveCurrentProject>(action) && !CurrentProjectPath) action = Action::ShowSaveProjectDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_finalize_gesture |= std::holds_alternative<Action::ToggleValue>(action);

        const bool is_savable = action.IsSavable();
        if (is_savable) store::BeginTransient(); // Idempotent.
        // todo really we want to separate out stateful and non-stateful actions, and commit each batch of stateful actions.
        else if (!stateful_actions.empty()) throw std::runtime_error("Non-stateful action in the same batch as stateful action (in transient mode).");

        Match(
            action,
            [&](const Action::AppAction &a) { app.Apply(a); },
            [&](const Action::ProjectAction &a) { Apply(a); },
        );

        Match(
            action,
            [&](const Action::StatefulAction &a) { stateful_actions.emplace_back(a, action_moment.second); },
            // Note: `const auto &` capture does not work when the other type is itself a variant group. Need to be exhaustive.
            [&](const Action::NonStatefulAction &) {},
        );
    }

    const bool finalize = force_finalize_gesture || (!Stateful::Field::IsGesturing && !History.ActiveGesture.empty() && History.GestureTimeRemainingSec(application_settings.GestureDurationSec) <= 0);
    if (!stateful_actions.empty()) {
        const auto &patch = store::CheckedCommit();
        OnPatch(patch);
        History.ActiveGesture.insert(History.ActiveGesture.end(), stateful_actions.begin(), stateful_actions.end());
        History.UpdateGesturePaths(stateful_actions, patch);
    } else {
        store::Commit(); // This ends transient mode but should not modify the state, since there were no stateful actions.
    }
    if (finalize) History.FinalizeGesture();
}

void q(const Action::Any &&action, bool flush) {
    ActionQueue.enqueue({action, Clock::now()});
    if (flush) Project::RunQueuedActions(true); // If the `flush` flag is set, we finalize the gesture now.
}

#define DefineQ(ActionType)                                                                                          \
    void Action::ActionType::q(bool flush) const { ::q(*this, flush); }                                              \
    void Action::ActionType::MenuItem() {                                                                            \
        if (ImGui::MenuItem(GetMenuLabel().c_str(), GetShortcut().c_str(), false, CanApply(Action::ActionType{}))) { \
            Action::ActionType{}.q();                                                                                \
        }                                                                                                            \
    }

DefineQ(Undo);
DefineQ(Redo);
DefineQ(SetHistoryIndex);
DefineQ(OpenProject);
DefineQ(OpenEmptyProject);
DefineQ(OpenDefaultProject);
DefineQ(SaveProject);
DefineQ(SaveDefaultProject);
DefineQ(SaveCurrentProject);
DefineQ(ShowOpenProjectDialog);
DefineQ(ShowSaveProjectDialog);
DefineQ(ToggleValue);
DefineQ(SetValue);
DefineQ(SetValues);
DefineQ(SetVector);
DefineQ(SetMatrix);
DefineQ(ApplyPatch);
DefineQ(SetImGuiColorStyle);
DefineQ(SetImPlotColorStyle);
DefineQ(SetFlowGridColorStyle);
DefineQ(SetGraphColorStyle);
DefineQ(SetGraphLayoutStyle);
DefineQ(ShowOpenFaustFileDialog);
DefineQ(ShowSaveFaustFileDialog);
DefineQ(ShowSaveFaustSvgFileDialog);
DefineQ(SaveFaustFile);
DefineQ(OpenFaustFile);
DefineQ(SaveFaustSvgFile);
DefineQ(FileDialogOpen);
DefineQ(FileDialogSelect);
DefineQ(FileDialogCancel);
