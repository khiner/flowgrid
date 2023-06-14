
#include "App.h"

#include "imgui_internal.h"
#include <range/v3/core.hpp>
#include <range/v3/view/transform.hpp>

#include "AppPreferences.h"
#include "Core/Action/Actions.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"
#include "Helper/File.h"
#include "Project/ProjectConstants.h"
#include "Project/ProjectJson.h"
#include "UI/UI.h"

using std::vector;

using namespace FlowGrid;

static std::optional<fs::path> CurrentProjectPath;
static bool ProjectHasChanges{false};

App::App(ComponentArgs &&args) : Component(std::move(args)) {
    Windows.SetWindowComponents({
        Audio,
        Settings,
        Audio.Faust.Code,
        Audio.Faust.Code.Metrics,
        Audio.Faust.Log,
        Audio.Faust.Graph,
        Audio.Faust.Params,
        Debug.StateViewer,
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

// using Any = Combine<Primitive::Any, Vector::Any, Matrix::Any, Store::Any, Audio::Any, FileDialog::Any, Style::Any>::type;
void App::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const PrimitiveField::ActionHandler::ActionType &a) { PrimitiveField::ActionHandler.Apply(a); },
        [](const VectorBase::ActionHandler::ActionType &a) { VectorBase::ActionHandler.Apply(a); },
        [](const MatrixBase::ActionHandler::ActionType &a) { MatrixBase::ActionHandler.Apply(a); },
        [](const store::ActionHandler::ActionType &a) { store::ActionHandler.Apply(a); },
        [&](const Audio::ActionType &a) { Audio.Apply(a); },
        [&](const FileDialog::ActionType &a) { FileDialog.Apply(a); },
        [&](const Windows::ActionType &a) { Windows.Apply(a); },
        [&](const Style::ActionType &a) { Style.Apply(a); },
    );
}

bool App::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [](const PrimitiveField::ActionHandler::ActionType &a) { return PrimitiveField::ActionHandler.CanApply(a); },
        [](const VectorBase::ActionHandler::ActionType &a) { return VectorBase::ActionHandler.CanApply(a); },
        [](const MatrixBase::ActionHandler::ActionType &a) { return MatrixBase::ActionHandler.CanApply(a); },
        [](const store::ActionHandler::ActionType &a) { return store::ActionHandler.CanApply(a); },
        [&](const Audio::ActionType &a) { return Audio.CanApply(a); },
        [&](const FileDialog::ActionType &a) { return FileDialog.CanApply(a); },
        [&](const Windows::ActionType &a) { return Windows.CanApply(a); },
        [&](const Style::ActionType &a) { return Style.CanApply(a); },
    );
}

void Apply(const Action::Savable &action) {
    Visit(
        action,
        [&](const App::ActionType &a) { app.Apply(a); },
        [&](const Project::ActionHandler::ActionType &a) { Project::ActionHandler.Apply(a); },
    );
}

void Apply(const Action::Any &action) {
    Visit(
        action,
        [](const App::ActionType &a) { app.Apply(a); },
        [](const Project::ActionHandler::ActionType &a) { Project::ActionHandler.Apply(a); },
    );
}

bool CanApply(const Action::Any &action) {
    return Visit(
        action,
        [](const App::ActionType &a) { return app.CanApply(a); },
        [](const Project::ActionHandler::ActionType &a) { return Project::ActionHandler.CanApply(a); },
    );
}

using namespace ImGui;

void App::Render() const {
    static const auto Shortcuts = Action::Any::CreateShortcuts();

    const auto &io = GetIO();
    for (const auto &[action_id, shortcut] : Shortcuts) {
        const auto &[mod, key] = shortcut.Parsed;
        if (mod == io.KeyMods && IsKeyPressed(GetKeyIndex(ImGuiKey(key)))) {
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

        Audio.Faust.Code.Dock(faust_editor_node_id);
        Audio.Faust.Code.Metrics.Dock(dockspace_id); // What's remaining of the main dockspace after splitting is used for the editor metrics.
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
        if (child == &Windows) continue;
        if (auto *drawable_child = dynamic_cast<const Drawable *>(child)) {
            if (!Windows.IsVisible(child->Id)) {
                drawable_child->Draw();
            }
        }
    }

    Windows.Draw();

    static string PrevSelectedPath = "";
    if (PrevSelectedPath != FileDialog.SelectedFilePath) {
        const fs::path selected_path = string(FileDialog.SelectedFilePath);
        const string &extension = selected_path.extension();
        if (AllProjectExtensions.find(extension) != AllProjectExtensions.end()) {
            if (FileDialog.SaveMode) Action::Project::Save{selected_path}.q();
            else Action::Project::Open{selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }
}

void App::OpenRecentProjectMenuItem() {
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
    if (!IsUserProjectPath(path)) return;

    ProjectHasChanges = false;
    CurrentProjectPath = path;
    Preferences.OnProjectOpened(path);
}

std::optional<ProjectJsonFormat> GetProjectJsonFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (!ProjectJsonFormatForExtension.contains(ext)) return {};
    return ProjectJsonFormatForExtension.at(ext);
}

bool Project::Save(const fs::path &path) {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !ProjectHasChanges) return false;

    const auto format = GetProjectJsonFormat(path);
    if (!format) return false; // TODO log

    History.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, GetProjectJson(*format).dump())) return false; // TODO log

    SetCurrentProjectPath(path);
    return true;
}

void Project::SaveEmpty() { Save(EmptyProjectPath); }

void OnPatch(const Patch &patch) {
    if (patch.Empty()) return;

    History.LatestUpdatedPaths = patch.Ops | views::transform([&patch](const auto &entry) { return patch.BasePath / entry.first; }) | ranges::to<vector>;
    ProjectHasChanges = true;

    static std::set<Field *> modified_fields;
    modified_fields.clear();
    for (const auto &path : History.LatestUpdatedPaths) {
        // Find all updated fields, including container fields.
        auto modified_field = Field::WithPath.find(path);
        if (modified_field == Field::WithPath.end()) modified_field = Field::WithPath.find(path.parent_path());
        if (modified_field == Field::WithPath.end()) modified_field = Field::WithPath.find(path.parent_path().parent_path());
        if (modified_field == Field::WithPath.end()) throw std::runtime_error(std::format("`SetStore` resulted in a patch affecting a path belonging to an unknown field: {}", path.string()));

        modified_fields.emplace(modified_field->second);

        // TODO Only update contexts when not ui-initiated (via a an `ApplyPatch` action inside the `WantSaveIniSettings` block).
        //   Otherwise it's redundant.
        if (path.string().rfind(imgui_settings.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImGuiSettings;
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
    Field::IsGesturing = false;
}

void Project::ActionHandler::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Project::ShowOpenDialog &) { file_dialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", ""}); },
        [](const Action::Project::ShowSaveDialog &) { file_dialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
        [](const Action::Project::OpenEmpty &) { Open(EmptyProjectPath); },
        [](const Action::Project::Open &a) { Open(a.path); },
        [](const Action::Project::OpenDefault &) { Open(DefaultProjectPath); },

        [](const Action::Project::Save &a) { Save(a.path); },
        [](const Action::Project::SaveDefault &) { Save(DefaultProjectPath); },
        [](const Action::Project::SaveCurrent &) {
            if (CurrentProjectPath) Save(*CurrentProjectPath);
        },
        // History-changing actions:
        [](const Action::Project::Undo &) {
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
        [](const Action::Project::Redo &) { SetHistoryIndex(History.Index + 1); },
        [](const Action::Project::SetHistoryIndex &a) { SetHistoryIndex(a.index); },
    );
}

bool Project::ActionHandler::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [](const Action::Project::Undo &) { return History.CanUndo(); },
        [](const Action::Project::Redo &) { return History.CanRedo(); },
        [](const Action::Project::Save &) { return !History.Empty(); },
        [](const Action::Project::SaveDefault &) { return !History.Empty(); },
        [](const Action::Project::ShowSaveDialog &) { return ProjectHasChanges; },
        [](const Action::Project::SaveCurrent &) { return ProjectHasChanges; },
        [](const Action::Project::OpenDefault &) { return fs::exists(DefaultProjectPath); },
        [](const auto &) { return true; },
    );
}

void Project::Open(const fs::path &path) {
    const auto format = GetProjectJsonFormat(path);
    if (!format) return; // TODO log

    Init();

    const nlohmann::json project = nlohmann::json::parse(FileIO::read(path));
    if (format == StateFormat) {
        OnPatch(store::CheckedSetJson(project));
        History = {};
    } else if (format == ActionFormat) {
        Open(EmptyProjectPath); // Intentional recursive call.

        const StoreHistory::IndexedGestures indexed_gestures = project;
        store::BeginTransient();
        for (const auto &gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture) ::Apply(action_moment.first);
            History.AddTransient(gesture);
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

using Action::ActionMoment, Action::SavableActionMoment;
inline static moodycamel::BlockingConcurrentQueue<ActionMoment> ActionQueue;

void RunQueuedActions(bool force_finalize_gesture) {
    static ActionMoment action_moment;
    static vector<SavableActionMoment> stateful_actions; // Same type as `Gesture`, but doesn't represent a full semantic "gesture".
    stateful_actions.clear();

    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, _] = action_moment;
        if (!CanApply(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can tell us where to save it:
        if (std::holds_alternative<Action::Project::SaveCurrent>(action) && !CurrentProjectPath) action = Action::Project::ShowSaveDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_finalize_gesture |= std::holds_alternative<Action::Primitive::ToggleBool>(action);

        const bool is_savable = action.IsSavable();
        if (is_savable) store::BeginTransient(); // Idempotent.
        // todo really we want to separate out stateful and non-stateful actions, and commit each batch of stateful actions.
        else if (!stateful_actions.empty()) throw std::runtime_error("Non-stateful action in the same batch as stateful action (in transient mode).");

        Apply(action);

        Visit(
            action,
            [](const Action::Savable &a) { stateful_actions.emplace_back(a, action_moment.second); },
            // Note: `const auto &` capture does not work when the other type is itself a variant group. Need to be exhaustive.
            [](const Action::NonSavable &) {},
        );
    }

    const bool finalize = force_finalize_gesture || (!Field::IsGesturing && !History.ActiveGesture.empty() && History.GestureTimeRemainingSec(application_settings.GestureDurationSec) <= 0);
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
    if (flush) RunQueuedActions(true); // If the `flush` flag is set, we finalize the gesture now.
}

#define DefineQ(ActionType)                                                                                          \
    void Action::ActionType::q(bool flush) const { ::q(*this, flush); }                                              \
    void Action::ActionType::MenuItem() {                                                                            \
        if (ImGui::MenuItem(GetMenuLabel().c_str(), GetShortcut().c_str(), false, CanApply(Action::ActionType{}))) { \
            Action::ActionType{}.q();                                                                                \
        }                                                                                                            \
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
DefineQ(Primitive::ToggleBool);
DefineQ(Primitive::Set);
DefineQ(Primitive::SetMany);
DefineQ(Vector::Set);
DefineQ(Matrix::Set);
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
DefineQ(FileDialog::Cancel);
