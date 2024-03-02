
#include "Project.h"

#include "imgui_internal.h"
#include <format>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>
#include <set>

#include "Application/ApplicationPreferences.h"
#include "Core/Action/ActionMenuItem.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"
#include "Helper/File.h"
#include "Helper/String.h"
#include "Helper/Time.h"

using namespace FlowGrid;

static SavedActionMoments ActiveGestureActions{}; // uncompressed, uncommitted

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
    if (auto it = ProjectFormatByExtension.find(ext); it != ProjectFormatByExtension.end()) return it->second;
    return {};
}

static float GestureTimeRemainingSec(float gesture_duration_sec) {
    if (ActiveGestureActions.empty()) return 0;
    return std::max(0.f, gesture_duration_sec - fsec(Clock::now() - ActiveGestureActions.back().QueueTime).count());
}

Project::Project(Store &store, PrimitiveActionQueuer &primitive_q, ActionProducer<ProducedActionType>::Enqueue q)
    : Component(store, primitive_q, Windows, Style), ActionableProducer(std::move(q)),
      HistoryPtr(std::make_unique<StoreHistory>(store)), History(*HistoryPtr) {
    Windows.SetWindowComponents({
        Audio.Graph,
        Audio.Graph.Connections,
        Audio.Style,
        Settings,
        Audio.Faust.FaustDsps,
        Audio.Faust.Logs,
        Audio.Faust.Graphs,
        Audio.Faust.Paramss,
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

Project::~Project() = default;

void Project::RefreshChanged(const Patch &patch, bool add_to_gesture) {
    MarkAllChanged(patch);
    static std::unordered_set<ChangeListener *> affected_listeners;

    // Find listeners to notify.
    for (const auto id : ChangedIds) {
        if (!FieldIds.contains(id)) continue; // The component was deleted.

        ById.at(id)->Refresh();

        const auto &listeners = ChangeListenersById[id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    // Find ancestor listeners to notify.
    // (Listeners can disambiguate by checking `IsChanged(bool include_descendents = false)` and `IsDescendentChanged()`.)
    for (const auto id : ChangedAncestorComponentIds) {
        if (!ById.contains(id)) continue; // The component was deleted.

        const auto &listeners = ChangeListenersById[id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    for (auto *listener : affected_listeners) listener->OnComponentChanged();
    affected_listeners.clear();

    // Update gesture paths.
    if (add_to_gesture) {
        for (const auto &[field_id, paths_moment] : ChangedPaths) {
            GestureChangedPaths[field_id].push_back(paths_moment);
        }
    }
}

Component *Project::FindChanged(ID component_id, const std::vector<PatchOp> &ops) {
    if (auto it = ById.find(component_id); it != ById.end()) {
        auto *component = it->second;
        if (ops.size() == 1 && (ops.front().Op == PatchOpType::Add || ops.front().Op == PatchOpType::Remove)) {
            // Do not mark any components as added/removed if they are within a container.
            // The container's auxiliary component is marked as changed instead (and its ID will be in same patch).
            if (auto *container = FindAncestorContainer(*component)) return nullptr;
        }
        // When a container's auxiliary component is changed, mark the container as changed instead.
        if (ContainerAuxiliaryIds.contains(component_id)) return component->Parent;
        return component;
    }

    return nullptr;
}

void Project::MarkAllChanged(const Patch &patch) {
    const auto change_time = Clock::now();
    ClearChanged();

    for (const auto &[id, ops] : patch.Ops) {
        if (auto *changed = FindChanged(id, ops)) {
            const ID id = changed->Id;
            const auto path = changed->Path;
            ChangedPaths[id].first = change_time;
            ChangedPaths[id].second.insert(path); // todo build path for containers from ops.

            // Mark the changed field and all its ancestors.
            ChangedIds.insert(id);
            for (const auto *ancestor = changed->Parent; ancestor != nullptr; ancestor = ancestor->Parent) {
                ChangedAncestorComponentIds.insert(ancestor->Id);
            }
        }
    }

    // Copy `ChangedPaths` over to `LatestChangedPaths`.
    // (`ChangedPaths` is cleared at the end of each action, while `LatestChangedPaths` is retained for the lifetime of the application.)
    for (const auto &[field_id, paths_moment] : ChangedPaths) LatestChangedPaths[field_id] = paths_moment;
}

void Project::CommitGesture() const {
    GestureChangedPaths.clear();
    if (ActiveGestureActions.empty()) return;

    const auto merged_actions = MergeActions(ActiveGestureActions);
    ActiveGestureActions.clear();
    if (merged_actions.empty()) return;

    History.AddGesture({merged_actions, Clock::now()}, Id);
}

void Project::SetHistoryIndex(u32 index) const {
    if (index == History.Index) return;

    GestureChangedPaths.clear();
    // If we're mid-gesture, revert the current gesture before navigating to the new index.
    ActiveGestureActions.clear();
    History.SetIndex(index);
    const auto patch = RootStore.CheckedSet(History.CurrentStore(), Id);
    RefreshChanged(patch);
    // ImGui settings are cheched separately from style since we don't need to re-apply ImGui settings state to ImGui context
    // when it initially changes, since ImGui has already updated its own context.
    // We only need to update the ImGui context based on settings changes when the history index changes.
    // However, style changes need to be applied to the ImGui context in all cases, since these are issued from component changes.
    // We don't make `ImGuiSettings` a component change listener for this because it would would end up being slower,
    // since it has many descendents, and we would wastefully check for changes during the forward action pass, as explained above.
    // xxx how to update to patches using IDs instead of paths? Check every ImGuiSettings descendent ID?
    // if (patch.IsPrefixOfAnyPath(ImGuiSettings.Path)) ImGuiSettings::IsChanged = true;
    ImGuiSettings::IsChanged = true;
    ProjectHasChanges = true;
}

json Project::GetProjectJson(const ProjectFormat format) const {
    switch (format) {
        case StateFormat: return ToJson();
        case ActionFormat: return History.GetIndexedGestures();
    }
}

// Note: If/when we support arbitrary json actions, we'll need to check primitive/container types.
//   Maybe with a separate `Find` for each type?
//   Could also have each primitive accept an `Action::Primitive::Any`,
//   and do the best it can to convert it to something meaningful (e.g. convert string set to an int set).
void Project::ApplyPrimitiveAction(const Action::Primitive::Any &action) const {
    const auto *prim = ById.at(action.GetComponentId());
    std::visit(
        Match{
            [&prim](const Bool::ActionType &a) { static_cast<const Bool *>(prim)->Apply(a); },
            [&prim](const Int::ActionType &a) { static_cast<const Int *>(prim)->Apply(a); },
            [&prim](const UInt::ActionType &a) { static_cast<const UInt *>(prim)->Apply(a); },
            [&prim](const Float::ActionType &a) { static_cast<const Float *>(prim)->Apply(a); },
            [&prim](const Enum::ActionType &a) { static_cast<const Enum *>(prim)->Apply(a); },
            [&prim](const Flags::ActionType &a) { static_cast<const Flags *>(prim)->Apply(a); },
            [&prim](const String::ActionType &a) { static_cast<const String *>(prim)->Apply(a); },
        },
        action
    );
}

void Project::ApplyContainerAction(const Action::Container::Any &action) const {
    const auto *container = ById.at(action.GetComponentId());
    std::visit(
        Match{
            [&container](const AdjacencyList::ActionType &a) { static_cast<const AdjacencyList *>(container)->Apply(a); },
            [&container](const Navigable<u32>::ActionType &a) { static_cast<const Navigable<u32> *>(container)->Apply(a); },
            [&container](const Vec2::ActionType &a) { static_cast<const Vec2 *>(container)->Apply(a); },
            [&container](const PrimitiveSet<u32>::ActionType &a) { static_cast<const PrimitiveSet<u32> *>(container)->Apply(a); },
            [&container](const PrimitiveVector<bool>::ActionType &a) { static_cast<const PrimitiveVector<bool> *>(container)->Apply(a); },
            [&container](const PrimitiveVector<int>::ActionType &a) { static_cast<const PrimitiveVector<int> *>(container)->Apply(a); },
            [&container](const PrimitiveVector<u32>::ActionType &a) { static_cast<const PrimitiveVector<u32> *>(container)->Apply(a); },
            [&container](const PrimitiveVector<float>::ActionType &a) { static_cast<const PrimitiveVector<float> *>(container)->Apply(a); },
            [&container](const PrimitiveVector<std::string>::ActionType &a) { static_cast<const PrimitiveVector<std::string> *>(container)->Apply(a); },
        },
        action
    );
}

void Project::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Primitive::Any &a) { ApplyPrimitiveAction(a); },
            [this](const Action::Container::Any &a) { ApplyContainerAction(a); },
            [](const Action::TextBuffer::Any &a) {
                const auto *buffer = ById.at(a.GetComponentId());
                static_cast<const TextBuffer *>(buffer)->Apply(a);
            },

            [this](const Action::Project::OpenEmpty &) { Open(EmptyProjectPath); },
            [this](const Action::Project::Open &a) { Open(a.file_path); },
            [this](const Action::Project::OpenDefault &) { Open(DefaultProjectPath); },

            [this](const Action::Project::Save &a) { Save(a.file_path); },
            [this](const Action::Project::SaveDefault &) { Save(DefaultProjectPath); },
            [this](const Action::Project::SaveCurrent &) {
                if (CurrentProjectPath) Save(*CurrentProjectPath);
            },
            // History-changing actions:
            [this](const Action::Project::Undo &) {
                if (History.Empty()) return;

                // `StoreHistory::SetIndex` reverts the current gesture before applying the new history index.
                // If we're at the end of the stack, we want to commit the active gesture and add it to the stack.
                // Otherwise, if we're already in the middle of the stack somewhere, we don't want an active gesture
                // to commit and cut off everything after the current history index, so an undo just ditches the active changes.
                // (This allows consistent behavior when e.g. being in the middle of a change and selecting a point in the undo history.)
                if (History.Index == History.Size() - 1) {
                    if (!ActiveGestureActions.empty()) CommitGesture();
                    SetHistoryIndex(History.Index - 1);
                } else {
                    SetHistoryIndex(History.Index - (ActiveGestureActions.empty() ? 1 : 0));
                }
            },
            [this](const Action::Project::Redo &) { SetHistoryIndex(History.Index + 1); },
            [this](const Action::Project::SetHistoryIndex &a) { SetHistoryIndex(a.index); },

            [this](const Store::ActionType &a) { RootStore.Apply(a); },
            [this](const Action::Project::ShowOpenDialog &) {
                FileDialog.Set({
                    .owner_id = Id,
                    .title = "Choose file",
                    .filters = AllProjectExtensionsDelimited,
                });
            },
            [this](const Action::Project::ShowSaveDialog &) { FileDialog.Set({Id, "Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
            [this](const Audio::ActionType &a) { Audio.Apply(a); },
            [this](const FileDialog::ActionType &a) { FileDialog.Apply(a); },
            [this](const Windows::ActionType &a) { Windows.Apply(a); },
            [this](const fg::Style::ActionType &a) { Style.Apply(a); },
        },
        action
    );
}

bool Project::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [](const Action::Primitive::Any &) { return true; },
            [](const Action::Container::Any &) { return true; },
            [](const Action::TextBuffer::Any &) { return true; },

            [this](const Action::Project::Undo &) { return !ActiveGestureActions.empty() || History.CanUndo(); },
            [this](const Action::Project::Redo &) { return History.CanRedo(); },
            [this](const Action::Project::SetHistoryIndex &a) { return a.index < History.Size(); },
            [this](const Action::Project::Save &) { return !History.Empty(); },
            [this](const Action::Project::SaveDefault &) { return !History.Empty(); },
            [](const Action::Project::ShowOpenDialog &) { return true; },
            [](const Action::Project::ShowSaveDialog &) { return ProjectHasChanges; },
            [](const Action::Project::SaveCurrent &) { return ProjectHasChanges; },
            [](const Action::Project::OpenDefault &) { return fs::exists(DefaultProjectPath); },
            [](const Action::Project::OpenEmpty &) { return true; },
            [](const Action::Project::Open &) { return true; },

            [this](const Store::ActionType &a) { return RootStore.CanApply(a); },
            [this](const Audio::ActionType &a) { return Audio.CanApply(a); },
            [this](const FileDialog::ActionType &a) { return FileDialog.CanApply(a); },
            [this](const Windows::ActionType &a) { return Windows.CanApply(a); },
            [this](const fg::Style::ActionType &a) { return Style.CanApply(a); },
        },
        action
    );
}

using namespace ImGui;

static bool IsPressed(ImGuiKeyChord chord) {
    return IsKeyChordPressed(chord, ImGuiKeyOwner_None, ImGuiInputFlags_Repeat);
}

std::optional<Project::ActionType> Project::ProduceKeyboardAction() const {
    using namespace Action::Project;

    if (IsPressed(ImGuiMod_Super | ImGuiKey_N)) return OpenEmpty{};
    if (IsPressed(ImGuiMod_Super | ImGuiKey_O)) return ShowOpenDialog{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Super | ImGuiKey_S)) return ShowSaveDialog{};
    if (IsPressed(ImGuiMod_Super | ImGuiKey_Z)) return Undo{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Super | ImGuiKey_Z)) return Redo{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Super | ImGuiKey_O)) return OpenDefault{};
    if (IsPressed(ImGuiMod_Super | ImGuiKey_S)) return SaveCurrent{};

    return {};
}

void Project::Render() const {
    MainMenu.Draw();

    // Good initial layout setup example in this issue: https://github.com/ocornut/imgui/issues/3548
    auto dockspace_id = DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    int frame_count = GetCurrentContext()->FrameCount;
    if (frame_count == 1) {
        auto audio_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        auto utilities_node_id = DockBuilderSplitNode(audio_node_id, ImGuiDir_Down, 0.5f, nullptr, &audio_node_id);

        auto debug_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
        auto metrics_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Right, 0.35f, nullptr, &debug_node_id);

        auto info_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);
        auto settings_node_id = DockBuilderSplitNode(info_node_id, ImGuiDir_Down, 0.25f, nullptr, &info_node_id);
        auto faust_tools_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.5f, nullptr, &dockspace_id);
        auto faust_graph_node_id = DockBuilderSplitNode(faust_tools_node_id, ImGuiDir_Left, 0.5f, nullptr, &faust_tools_node_id);
        DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.5f, nullptr, &dockspace_id); // text editor

        Audio.Graph.Dock(audio_node_id);
        Audio.Graph.Connections.Dock(audio_node_id);
        Audio.Style.Dock(audio_node_id);

        Audio.Faust.FaustDsps.Dock(dockspace_id);
        Audio.Faust.Graphs.Dock(faust_graph_node_id);
        Audio.Faust.Paramss.Dock(faust_tools_node_id);
        Audio.Faust.Logs.Dock(faust_tools_node_id);

        Debug.Dock(debug_node_id);
        Debug.ProjectPreview.Dock(debug_node_id);
        Debug.StorePathUpdateFrequency.Dock(debug_node_id);
        Debug.DebugLog.Dock(debug_node_id);
        Debug.StackTool.Dock(debug_node_id);
        Debug.Metrics.Dock(metrics_node_id);

        Style.Dock(utilities_node_id);
        Demo.Dock(utilities_node_id);

        Info.Dock(info_node_id);
        Settings.Dock(settings_node_id);
    }

    // Draw non-window children.
    for (const auto *child : Children) {
        if (!Windows.IsWindow(child->Id) && child != &Windows) child->Draw();
    }

    Windows.Draw();

    if (frame_count == 1) {
        // Default focused windows.
        Style.Focus();
        Audio.Graph.Focus();
        Audio.Faust.Graphs.Focus();
        Audio.Faust.Paramss.Focus();
        Debug.Focus(); // not visible by default anymore
    }

    // Handle file dialog.
    static string PrevSelectedPath = "";
    if (PrevSelectedPath != FileDialog.SelectedFilePath && FileDialog.OwnerId == Id) {
        const fs::path selected_path = FileDialog.SelectedFilePath;
        PrevSelectedPath = FileDialog.SelectedFilePath = "";
        if (FileDialog.SaveMode) Q(Action::Project::Save{selected_path});
        else Q(Action::Project::Open{selected_path});
    }

    if (auto action = ProduceKeyboardAction()) Q(*action);
}

void Project::OpenRecentProjectMenuItem() const {
    if (BeginMenu("Open recent project", !Preferences.RecentlyOpenedPaths.empty())) {
        for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
            if (MenuItem(recently_opened_path.filename().c_str())) Q(Action::Project::Open{recently_opened_path});
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
    if (!FileIO::write(path, GetProjectJson(*format).dump())) {
        throw std::runtime_error(std::format("Failed to write project file: {}", path.string()));
    }

    SetCurrentProjectPath(path);
    return true;
}

void Project::OnApplicationLaunch() const {
    Component::IsGesturing = false;
    History.Clear();
    ClearChanged();
    LatestChangedPaths.clear();

    // When loading a new project, we always refresh all UI contexts.
    Style.ImGui.IsChanged = true;
    Style.ImPlot.IsChanged = true;
    ImGuiSettings::IsChanged = true;

    // Keep the canonical "empty" project up-to-date.
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);
    Save(EmptyProjectPath);
}

static json ReadFileJson(const fs::path &file_path) { return json::parse(FileIO::read(file_path)); }

// Helper function used in `Project::Open`.
// Modifies the active transient store.
void Project::OpenStateFormatProject(const fs::path &file_path) const {
    auto j = ReadFileJson(file_path);
    // First, refresh all component containers to ensure the dynamically managed component instances match the JSON.
    for (const ID auxiliary_id : ContainerAuxiliaryIds) {
        if (auto *auxiliary_field = ById.at(auxiliary_id); j.contains(auxiliary_field->JsonPointer())) {
            auxiliary_field->SetJson(std::move(j.at(auxiliary_field->JsonPointer())));
            auxiliary_field->Refresh();
            auxiliary_field->Parent->Refresh();
        }
    }

    // Now, every flattened JSON pointer is 1:1 with an instance path.
    SetJson(std::move(j));

    // We could do `RefreshChanged(RootStore.CheckedCommit(Id))`, and only refresh the changed components,
    // but this gets tricky with component containers, since the store patch will contain added/removed paths
    // that have already been accounted for above.
    RootStore.Commit();
    ClearChanged();
    LatestChangedPaths.clear();
    RefreshAll();

    // Always update the ImGui context, regardless of the patch, to avoid expensive sifting through paths and just to be safe.
    ImGuiSettings.IsChanged = true;
    History.Clear();
}

void Project::Open(const fs::path &file_path) const {
    const auto format = GetProjectFormat(file_path);
    if (!format) return; // TODO log

    Component::IsGesturing = false;

    if (format == StateFormat) {
        OpenStateFormatProject(file_path);
    } else if (format == ActionFormat) {
        OpenStateFormatProject(EmptyProjectPath);

        StoreHistory::IndexedGestures indexed_gestures = ReadFileJson(file_path);
        for (auto &gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture.Actions) {
                std::visit(Match{[this](const Project::ActionType &a) { Apply(a); }}, action_moment.Action);
                RefreshChanged(RootStore.CheckedCommit(Id));
            }
            History.AddGesture(std::move(gesture), Id);
        }
        SetHistoryIndex(indexed_gestures.Index);
        LatestChangedPaths.clear();
    }

    SetCurrentProjectPath(file_path);
}

void Project::WindowMenuItem() const {
    const auto &item = [this](const Component &c) { return Windows.ToggleMenuItem(c); };
    if (BeginMenu("Windows")) {
        if (BeginMenu("Audio")) {
            item(Audio.Graph);
            item(Audio.Graph.Connections);
            item(Audio.Style);
            EndMenu();
        }
        if (BeginMenu("Faust")) {
            item(Audio.Faust.FaustDsps);
            item(Audio.Faust.Graphs);
            item(Audio.Faust.Paramss);
            item(Audio.Faust.Logs);
            EndMenu();
        }
        if (BeginMenu("Debug")) {
            item(Debug);
            item(Debug.ProjectPreview);
            item(Debug.StorePathUpdateFrequency);
            item(Debug.DebugLog);
            item(Debug.StackTool);
            item(Debug.Metrics);
            EndMenu();
        }
        item(Style);
        item(Demo);
        item(Info);
        item(Settings);
        EndMenu();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Debug
//-----------------------------------------------------------------------------

#include "date.h"
#include "implot.h"

#include <range/v3/view/concat.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <ranges>

#include "UI/HelpMarker.h"
#include "UI/JsonTree.h"

Plottable Project::StorePathChangeFrequencyPlottable() const {
    if (History.GetChangedPathsCount() == 0 && GestureChangedPaths.empty()) return {};

    std::map<StorePath, u32> gesture_change_counts;
    for (const auto &[id, changed_paths] : GestureChangedPaths) {
        const auto &component = ById.at(id);
        for (const auto &paths_moment : changed_paths) {
            for (const auto &path : paths_moment.second) {
                gesture_change_counts[path == "" ? component->Path : component->Path / path]++;
            }
        }
    }

    const auto history_change_counts = History.GetChangeCountById() | std::views::transform([](const auto &entry) { return std::pair(ById.at(entry.first)->Path, entry.second); }) | ranges::to<std::map>;
    const std::set<StorePath> paths = ranges::views::concat(ranges::views::keys(history_change_counts), ranges::views::keys(gesture_change_counts)) | ranges::to<std::set>;

    u32 i = 0;
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
    auto [labels, values] = GetProject().StorePathChangeFrequencyPlottable();
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
    ShowIDStackToolWindow();
}

void Project::Debug::Metrics::ImGuiMetrics::Render() const { ImGui::ShowMetricsWindow(); }
void Project::Debug::Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

using namespace FlowGrid;

void Project::Debug::OnComponentChanged() {
    if (AutoSelect.IsChanged()) {
        WindowFlags = AutoSelect ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None;
    }
}

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

    json project_json = GetProject().GetProjectJson(ProjectFormat(int(Format)));
    if (Raw) {
        TextUnformatted(project_json.dump(4).c_str());
    } else {
        SetNextItemOpen(true);
        fg::JsonTree("", std::move(project_json));
    }
}

void ShowActions(const SavedActionMoments &actions) {
    for (u32 action_index = 0; action_index < actions.size(); action_index++) {
        const auto &[action, queue_time] = actions[action_index];
        if (TreeNodeEx(std::to_string(action_index).c_str(), ImGuiTreeNodeFlags_None, "%s", action.GetPath().string().c_str())) {
            BulletText("Queue time: %s", date::format("%Y-%m-%d %T", queue_time).c_str());
            SameLine();
            fg::HelpMarker("The original queue time of the action. If this is a merged action, this is the queue time of the most recent action in the merge.");
            json data = json(action)[1];
            if (!data.is_null()) {
                SetNextItemOpen(true);
                JsonTree("Data", std::move(data));
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
    const auto &project = GetProject();
    {
        // Active (uncompressed) gesture
        const bool is_gesturing = Component::IsGesturing, any_gesture_actions = !ActiveGestureActions.empty();
        if (any_gesture_actions || is_gesturing) {
            // Gesture completion progress bar (full-width to empty).
            const float gesture_duration_sec = project.Settings.GestureDurationSec;
            const float time_remaining_sec = GestureTimeRemainingSec(gesture_duration_sec);
            const auto row_item_ratio_rect = RowItemRatioRect(time_remaining_sec / gesture_duration_sec);
            GetWindowDrawList()->AddRectFilled(row_item_ratio_rect.Min, row_item_ratio_rect.Max, GetFlowGridStyle().Colors[FlowGridCol_GestureIndicator]);

            const string active_gesture_title = std::format("Active gesture{}", any_gesture_actions ? " (uncompressed)" : "");
            if (TreeNodeEx(active_gesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (is_gesturing) FillRowItemBg(gStyle.ImGui.Colors[ImGuiCol_FrameBgActive]);
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
        const auto &history = project.History;
        const bool no_history = history.Empty();
        if (no_history) BeginDisabled();
        if (TreeNodeEx("History", ImGuiTreeNodeFlags_DefaultOpen, "History (Records: %d, Current record index: %d)", history.Size() - 1, history.Index)) {
            if (!no_history) {
                if (u32 edited_history_index = history.Index; SliderU32("History index", &edited_history_index, 0, history.Size() - 1)) {
                    project.Q(Action::Project::SetHistoryIndex{edited_history_index});
                }
            }
            for (u32 i = 1; i < history.Size(); i++) {
                // todo button to navitate to this history index.
                if (TreeNodeEx(std::to_string(i).c_str(), i == history.Index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &[store_record, gesture] = history.RecordAt(i);
                    BulletText("Gesture committed: %s\n", date::format("%Y-%m-%d %T", gesture.CommitTime).c_str());
                    if (TreeNode("Actions")) {
                        ShowActions(gesture.Actions);
                        TreePop();
                    }
                    if (TreeNode("Patch")) {
                        // We compute patches as we need them rather than memoizing.
                        const auto &patch = history.CreatePatch(i, project.Id);
                        for (const auto &[id, ops] : patch.Ops) {
                            const auto &path = ById.at(id)->Path;
                            if (TreeNodeEx(path.string().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                                for (const auto &op : ops) {
                                    BulletText("Op: %s", ToString(op.Op).c_str());
                                    if (op.Value) BulletText("Value: %s", json(*op.Value).dump().c_str());
                                    if (op.Old) BulletText("Old value: %s", json(*op.Old).dump().c_str());
                                }
                                TreePop();
                            }
                        }
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
        Text("Action variant size: %lu bytes", sizeof(Action::Saved));
        Text("Primitive variant size: %lu bytes", sizeof(PrimitiveVariant));
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

void Project::ApplyQueuedActions(ActionQueue<ActionType> &queue, bool force_commit_gesture, bool ignore_actions) const {
    static ActionMoment<ActionType> action_moment; // For dequeuing.

    if (ignore_actions) {
        while (queue.TryDequeue(action_moment)) {};
        return;
    }

    const bool gesture_actions_already_present = !ActiveGestureActions.empty();

    while (queue.TryDequeue(action_moment)) {
        auto &[action, queue_time] = action_moment;
        if (!CanApply(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can choose the save file:
        if (std::holds_alternative<Action::Project::SaveCurrent>(action) && !CurrentProjectPath) action = Action::Project::ShowSaveDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_commit_gesture |=
            std::holds_alternative<Action::Primitive::Bool::Toggle>(action) ||
            std::holds_alternative<Action::Vec2::ToggleLinked>(action) ||
            std::holds_alternative<Action::AdjacencyList::ToggleConnection>(action) ||
            std::holds_alternative<Action::FileDialog::Select>(action);

        Apply(action);

        std::visit(
            Match{
                [this, &store = RootStore, &queue_time](const Action::Saved &a) {
                    if (const auto patch = store.CheckedCommit(Id); !patch.Empty()) {
                        RefreshChanged(patch, true);
                        ActiveGestureActions.emplace_back(a, queue_time);
                        ProjectHasChanges = true;
                    }
                },
                // Note: `const auto &` capture does not work when the other type is itself a variant group - must be exhaustive.
                [](const Action::NonSaved &) {},
            },
            action
        );
    }

    if (force_commit_gesture ||
        (!Component::IsGesturing && gesture_actions_already_present && GestureTimeRemainingSec(Settings.GestureDurationSec) <= 0)) {
        CommitGesture();
    }
}
