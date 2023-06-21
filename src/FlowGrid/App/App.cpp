
#include "App.h"

#include "imgui_internal.h"
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include "AppPreferences.h"
#include "Core/Action/Actions.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"
#include "Helper/File.h"
#include "Project/ProjectJson.h"
#include "UI/UI.h"

using std::vector;
using namespace FlowGrid;

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

void App::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const FieldActionHandler::ActionType &a) { Field::ActionHandler.Apply(a); },
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
        [](const FieldActionHandler::ActionType &a) { return Field::ActionHandler.CanApply(a); },
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

// Project constants:
static const fs::path InternalPath = ".flowgrid";

static const std::map<ProjectJsonFormat, std::string> ExtensionForProjectJsonFormat{{ProjectJsonFormat::StateFormat, ".fls"}, {ProjectJsonFormat::ActionFormat, ".fla"}};
static const auto ProjectJsonFormatForExtension = ExtensionForProjectJsonFormat | std::views::transform([](const auto &p) { return std::pair(p.second, p.first); }) | ranges::to<std::map>();

static const std::set<std::string> AllProjectExtensions = std::views::keys(ProjectJsonFormatForExtension) | ranges::to<std::set>;
static const std::string AllProjectExtensionsDelimited = AllProjectExtensions | ranges::views::join(',') | ranges::to<std::string>;

static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectJsonFormat.at(ProjectJsonFormat::StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectJsonFormat.at(ProjectJsonFormat::ActionFormat));

static std::optional<fs::path> CurrentProjectPath;
static bool ProjectHasChanges{false};

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
            if (!Windows.IsWindow(child->Id)) {
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

    History.CommitGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, GetProjectJson(*format).dump())) return false; // TODO log

    SetCurrentProjectPath(path);
    return true;
}

void Project::SaveEmpty() { Save(EmptyProjectPath); }

void OnPatch(const Patch &patch) {
    if (patch.Empty()) return;

    History.LatestUpdatedPaths = patch.Ops | std::views::transform([&patch](const auto &entry) { return patch.BasePath / entry.first; }) | ranges::to<vector>;
    ProjectHasChanges = true;

    static std::set<Field *> modified_fields;
    modified_fields.clear();
    for (const auto &path : History.LatestUpdatedPaths) {
        auto *modified_field = Field::FindByPath(path);
        if (modified_field == nullptr) throw std::runtime_error(std::format("`Set` resulted in a patch affecting a path belonging to an unknown field: {}", path.string()));

        modified_fields.emplace(modified_field);

        // TODO Only update contexts when not ui-initiated (via a an `ApplyPatch` action inside the `WantSaveIniSettings` block).
        //   Otherwise it's redundant.
        if (path.string().rfind(imgui_settings.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImGuiSettings;
        else if (path.string().rfind(fg::style.ImGui.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImGuiStyle;
        else if (path.string().rfind(fg::style.ImPlot.Path.string(), 0) == 0) Ui.UpdateFlags |= UIContext::Flags_ImPlotStyle;
    }
    for (auto *modified_field : modified_fields) modified_field->RefreshValue();
}

void SetHistoryIndex(Count index) {
    History.SetIndex(index);
    OnPatch(store::CheckedSet(History.CurrentStore()));
}

void Project::Init() {
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);
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
        [](const Action::Project::Open &a) { Open(a.file_path); },
        [](const Action::Project::OpenDefault &) { Open(DefaultProjectPath); },

        [](const Action::Project::Save &a) { Save(a.file_path); },
        [](const Action::Project::SaveDefault &) { Save(DefaultProjectPath); },
        [](const Action::Project::SaveCurrent &) {
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
                if (!History.ActiveGestureActions.empty()) History.CommitGesture();
                ::SetHistoryIndex(History.Index - 1);
            } else {
                ::SetHistoryIndex(History.Index - (History.ActiveGestureActions.empty() ? 1 : 0));
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

void Project::Open(const fs::path &file_path) {
    const auto format = GetProjectJsonFormat(file_path);
    if (!format) return; // TODO log

    Init();

    const nlohmann::json project = nlohmann::json::parse(FileIO::read(file_path));
    if (format == StateFormat) {
        OnPatch(store::CheckedSetJson(project));
        History = {};
    } else if (format == ActionFormat) {
        Open(EmptyProjectPath); // Intentional recursive call.

        const StoreHistory::IndexedGestures indexed_gestures = project;
        store::BeginTransient();
        for (const auto &gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture.Actions) ::Apply(action_moment.first);
            History.AddTransientGesture(gesture);
        }
        OnPatch(store::CheckedCommit());
        ::SetHistoryIndex(indexed_gestures.Index);
    }

    SetCurrentProjectPath(file_path);
}

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

    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, queue_time] = action_moment;
        if (!CanApply(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can tell us where to save it:
        if (std::holds_alternative<Action::Project::SaveCurrent>(action) && !CurrentProjectPath) action = Action::Project::ShowSaveDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_commit_gesture |= std::holds_alternative<Action::Primitive::Bool::Toggle>(action);

        const bool is_savable = action.IsSavable();
        if (is_savable) store::BeginTransient(); // Idempotent.
        // todo really we want to separate out stateful and non-stateful actions, and commit each batch of stateful actions.
        else if (!stateful_actions.empty()) throw std::runtime_error("Non-stateful action in the same batch as stateful action (in transient mode).");

        Apply(action);

        Visit(
            action,
            [&queue_time](const Action::Savable &a) { stateful_actions.emplace_back(a, queue_time); },
            // Note: `const auto &` capture does not work when the other type is itself a variant group. Need to be exhaustive.
            [](const Action::NonSavable &) {},
        );
    }

    const bool commit_gesture = force_commit_gesture || (!Field::IsGesturing && !History.ActiveGestureActions.empty() && History.GestureTimeRemainingSec(application_settings.GestureDurationSec) <= 0);
    if (!stateful_actions.empty()) {
        const auto &patch = store::CheckedCommit();
        const auto commit_time = Clock::now();
        OnPatch(patch);
        History.OnStoreCommit(commit_time, stateful_actions, patch);
    } else {
        store::Commit(); // This ends transient mode but should not modify the state, since there were no stateful actions.
    }
    if (commit_gesture) History.CommitGesture();
}

#define DefineQ(ActionType)                                                                                          \
    void Action::ActionType::q() const { ::q(*this); }                                                               \
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
DefineQ(Primitive::Bool::Toggle);
DefineQ(Primitive::Int::Set);
DefineQ(Primitive::UInt::Set);
DefineQ(Primitive::Float::Set);
DefineQ(Primitive::String::Set);
DefineQ(Primitive::Enum::Set);
DefineQ(Primitive::Flags::Set);
DefineQ(MultilineString::Set);
DefineQ(Vector<bool>::Set);
DefineQ(Vector<int>::Set);
DefineQ(Vector<U32>::Set);
DefineQ(Vector<float>::Set);
DefineQ(Vector2D<bool>::Set);
DefineQ(Vector2D<int>::Set);
DefineQ(Vector2D<U32>::Set);
DefineQ(Vector2D<float>::Set);
DefineQ(Vec2::Set);
DefineQ(Vec2::SetX);
DefineQ(Vec2::SetY);
DefineQ(Vec2::SetAll);
DefineQ(Matrix<bool>::Set);
DefineQ(Matrix<bool>::SetValue);
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
