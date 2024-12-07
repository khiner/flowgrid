
#include "Project.h"

#include <format>
#include <ranges>
#include <set>

#include "imgui_internal.h"
#include "implot.h"

#include "Core/Action/ActionMenuItem.h"
#include "Core/Helper/File.h"
#include "Core/Helper/String.h"
#include "Core/Store/StoreHistory.h"
#include "Core/UI/HelpMarker.h"
#include "Core/UI/JsonTree.h"

struct Gesture {
    SavedActionMoments Actions;
    TimePoint CommitTime;
};

namespace nlohmann {
inline void to_json(json &j, const Action::Saved &action) {
    action.to_json(j);
}
inline void from_json(const json &j, Action::Saved &action) {
    Action::Saved::from_json(j, action);
}

Json(SavedActionMoment, Action, QueueTime);
Json(Gesture, Actions, CommitTime);
} // namespace nlohmann

using std::ranges::to, std::views::drop, std::views::join, std::views::keys, std::views::transform;

// Store history:
// Defining this here instead of its own impl file to have a single place that depends on Store.h and the fully-defined `Action::Any` type.
struct StoreHistory::Metrics {
    immer::map<ID, immer::vector<TimePoint>> CommitTimesById;

    void AddPatch(const Patch &patch, const TimePoint &commit_time) {
        for (ID id : patch.GetIds()) {
            auto commit_times = CommitTimesById.count(id) ? CommitTimesById.at(id).push_back(commit_time) : immer::vector<TimePoint>{commit_time};
            CommitTimesById = CommitTimesById.set(id, std::move(commit_times));
        }
    }
};

struct Record {
    PersistentStore Store;
    Gesture Gesture;
    StoreHistory::Metrics Metrics;
};
struct StoreHistory::Records {
    Records(const PersistentStore &initial_store) : Value{{initial_store, Gesture{{}, Clock::now()}, StoreHistory::Metrics{{}}}} {}

    std::vector<Record> Value;
};

StoreHistory::StoreHistory(const PersistentStore &store)
    : _Records(std::make_unique<Records>(store)), _Metrics(std::make_unique<Metrics>()) {}
StoreHistory::~StoreHistory() = default;

u32 StoreHistory::Size() const { return _Records->Value.size(); }

void StoreHistory::AddGesture(PersistentStore store, Gesture &&gesture, ID component_id) {
    const auto patch = CreatePatch(store, CurrentStore(), component_id);
    if (patch.Empty()) return;

    _Metrics->AddPatch(patch, gesture.CommitTime);

    while (Size() > Index + 1) _Records->Value.pop_back(); // todo use an undo _tree_ and keep this history
    _Records->Value.emplace_back(std::move(store), std::move(gesture), *_Metrics);
    Index = Size() - 1;
}
void StoreHistory::Clear(const PersistentStore &store) {
    Index = 0;
    _Records = std::make_unique<Records>(store);
    _Metrics = std::make_unique<Metrics>();
}
void StoreHistory::SetIndex(u32 new_index) {
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    Index = new_index;
    _Metrics = std::make_unique<Metrics>(_Records->Value[Index].Metrics);
}

const PersistentStore &StoreHistory::CurrentStore() const { return _Records->Value[Index].Store; }
const PersistentStore &StoreHistory::PrevStore() const { return _Records->Value[Index - 1].Store; }

std::map<ID, u32> StoreHistory::GetChangeCountById() const {
    return _Records->Value[Index].Metrics.CommitTimesById |
        transform([](const auto &entry) { return std::pair(entry.first, entry.second.size()); }) |
        to<std::map<ID, u32>>();
}
u32 StoreHistory::GetChangedPathsCount() const { return _Records->Value[Index].Metrics.CommitTimesById.size(); }

StoreHistory::ReferenceRecord StoreHistory::At(u32 index) const {
    const auto &record = _Records->Value[index];
    return {record.Store, record.Gesture};
}

Gestures StoreHistory::GetGestures() const {
    // The first record only holds the initial store with no gestures.
    return _Records->Value | drop(1) | transform([](const auto &record) { return record.Gesture; }) | to<std::vector>();
}

// Project constants:
static const fs::path InternalPath = ".flowgrid";
// Order matters here, as the first extension is the default project extension.
static const std::map<ProjectFormat, std::string_view> ExtensionByProjectFormat{
    {ProjectFormat::Action, ".fga"},
    {ProjectFormat::State, ".fgs"},
};
static const auto ProjectFormatByExtension = ExtensionByProjectFormat | transform([](const auto &pe) { return std::pair(pe.second, pe.first); }) | to<std::map>();
static const auto AllProjectExtensions = ProjectFormatByExtension | keys;
// todo this works with a trailing comma, but use `std::views::join_with` when clang supports it.
static const std::string AllProjectExtensionsDelimited = AllProjectExtensions | transform([](const auto &e) { return std::format("{}, ", e); }) | join | to<std::string>();

static const fs::path EmptyProjectPath = InternalPath / ("empty" + string(ExtensionByProjectFormat.at(ProjectFormat::State)));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
static const fs::path DefaultProjectPath = InternalPath / ("default" + string(ExtensionByProjectFormat.at(ProjectFormat::Action)));

static std::optional<ProjectFormat> GetProjectFormat(const fs::path &path) {
    if (auto it = ProjectFormatByExtension.find(std::string{path.extension()}); it != ProjectFormatByExtension.end()) {
        return it->second;
    }
    return {};
}

Project::Project(CreateApp &&create_app)
    : ActionableProducer(EnqueueFn([this](auto a) { return Queue.enqueue(EnqueueToken, {std::move(a), Clock::now()}); })),
      App(create_app({{&State, "App"}, SubProducer<AppActionType>(*this)})),
      HistoryPtr(std::make_unique<StoreHistory>(S)), History(*HistoryPtr) {
    // Initialize the global canonical store with all values set during project initialization.
    S = _S.Persistent();
    // Ensure all store values set during initialization are reflected in cached field/collection values, and any side effects are run.
    State.Refresh();
}

Project::~Project() = default;

void Project::RefreshChanged(Patch &&patch, bool add_to_gesture) const {
    MarkAllChanged(std::move(patch));

    std::unordered_set<ChangeListener *> affected_listeners;

    // Find listeners to notify.
    for (const auto id : ChangedIds) {
        if (!Component::ById.contains(id)) continue; // The component was deleted.

        Component::ById.at(id)->Refresh();

        const auto &listeners = ChangeListenersById[id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    // Find ancestor listeners to notify.
    // (Listeners can disambiguate by checking `IsChanged(bool include_descendents = false)` and `IsDescendentChanged()`.)
    for (const auto id : ChangedAncestorComponentIds) {
        if (!Component::ById.contains(id)) continue; // The component was deleted.

        const auto &listeners = ChangeListenersById[id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    for (auto *listener : affected_listeners) listener->OnComponentChanged();

    // Update gesture paths.
    if (add_to_gesture) {
        for (const auto &[field_id, paths_moment] : ChangedPaths) {
            GestureChangedPaths[field_id].push_back(paths_moment);
        }
    }
}

Component *Project::FindChanged(ID component_id, const std::vector<PatchOp> &ops) {
    if (auto it = Component::ById.find(component_id); it != Component::ById.end()) {
        auto *component = it->second;
        if (ops.size() == 1 && (ops.front().Op == PatchOpType::Add || ops.front().Op == PatchOpType::Remove)) {
            // Do not mark any components as added/removed if they are within a container.
            // The container's auxiliary component is marked as changed instead (and its ID will be in same patch).
            if (component->HasAncestorContainer()) return nullptr;
        }
        // When a container's auxiliary component is changed, mark the container as changed instead.
        if (Component::ContainerAuxiliaryIds.contains(component_id)) return component->Parent;
        return component;
    }

    return nullptr;
}

void Project::ClearChanged() const {
    ChangedPaths.clear();
    ChangedIds.clear();
    ChangedAncestorComponentIds.clear();
}

void Project::MarkAllChanged(Patch &&patch) const {
    const auto change_time = Clock::now();
    ClearChanged();

    for (const auto &[id, ops] : patch.Ops) {
        if (auto *changed = FindChanged(id, ops)) {
            const ID id = changed->Id;
            ChangedPaths[id].first = change_time;
            ChangedPaths.at(id).second.insert(changed->Path); // todo build path for containers from ops.

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

SavedActionMoments MergeActions(const SavedActionMoments &actions) {
    SavedActionMoments merged_actions; // Mutable return value.

    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const SavedActionMoment> active;
    for (u32 i = 0; i < actions.size(); i++) {
        if (!active) active.emplace(actions[i]);
        const auto &a = *active;
        const auto &b = actions[i + 1];
        const auto merge_result = a.Action.Merge(b.Action);
        std::visit(
            Match{
                [&](const bool cancel_out) {
                    if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                    else merged_actions.emplace_back(a); //
                    active.reset(); // No merge in either case. Move on to try compressing the next action.
                },
                [&](const Action::Saved &merged_action) {
                    // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
                    active.emplace(merged_action, b.QueueTime);
                },
            },
            merge_result
        );
    }
    if (active) merged_actions.emplace_back(*active);

    return merged_actions;
}

void Project::CommitGesture() const {
    GestureChangedPaths.clear();
    if (ActiveGestureActions.empty()) return;

    const auto merged_actions = MergeActions(ActiveGestureActions);
    ActiveGestureActions.clear();
    if (merged_actions.empty()) return;

    History.AddGesture(S, {merged_actions, Clock::now()}, State.Id);
}

void Project::SetHistoryIndex(u32 index) const {
    if (index == History.Index) return;

    GestureChangedPaths.clear();
    ActiveGestureActions.clear(); // In case we're mid-gesture, revert before navigating.
    History.SetIndex(index);
    const auto &store = History.CurrentStore();

    auto patch = CreatePatch(S, store, State.Id);
    // Overwrite persistent and transient stores with the provided store.
    S = store;
    _S = S.Transient();
    RefreshChanged(std::move(patch));
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

// Used for saving/loading the history.
// This is all the information needed to reconstruct a project.
struct IndexedGestures {
    Gestures Gestures;
    u32 Index;
};
Json(IndexedGestures, Gestures, Index);

json Project::GetProjectJson(ProjectFormat format) const {
    switch (format) {
        case ProjectFormat::State: return State.ToJson();
        case ProjectFormat::Action: return IndexedGestures{History.GetGestures(), History.Index};
    }
}

void Project::Apply(const ActionType &action) const {
    std::visit(
        Match{
            /* Project */
            [this](const Action::Project::OpenEmpty &) { Open(EmptyProjectPath); },
            [this](const Action::Project::Open &a) { Open(a.file_path); },
            [this](const Action::Project::OpenDefault &) { Open(DefaultProjectPath); },

            [this](const Action::Project::Save &a) { Save(a.file_path); },
            [this](const Action::Project::SaveDefault &) { Save(DefaultProjectPath); },
            [this](const Action::Project::SaveCurrent &) {
                if (CurrentProjectPath) Save(*CurrentProjectPath);
            },
            /* Project history */
            [this](const Action::Project::Undo &) {
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
            [this](const Action::Project::ShowOpenDialog &) {
                FileDialog.Set({
                    .OwnerId = State.Id,
                    .Title = "Choose file",
                    .Filters = AllProjectExtensionsDelimited,
                });
            },
            [this](const Action::Project::ShowSaveDialog &) { FileDialog.Set({State.Id, "Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
            /* File dialog */
            [this](const Action::FileDialog::Open &a) { FileDialog.SetJson(json::parse(a.dialog_json)); },
            // `SelectedFilePath` mutations are non-stateful side effects.
            [this](const Action::FileDialog::Select &a) { FileDialog.SelectedFilePath = a.file_path; },
            [this](const Action::Core::Any &a) { CoreHandler.Apply(a); },
            /* Store */
            [this](const Action::Store::ApplyPatch &a) {
                for (const auto &[id, ops] : a.patch.Ops) {
                    for (const auto &op : ops) {
                        if (op.Op == PatchOpType::PopBack) {
                            std::visit(
                                [&](auto &&v) {
                                    const auto vec = _S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id);
                                    _S.Set(id, vec.take(vec.size() - 1));
                                },
                                *op.Old
                            );
                        } else if (op.Op == PatchOpType::Remove) {
                            std::visit([&](auto &&v) { _S.Erase<std::decay_t<decltype(v)>>(id); }, *op.Old);
                        } else if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) {
                            std::visit([&](auto &&v) { _S.Set(id, std::move(v)); }, *op.Value);
                        } else if (op.Op == PatchOpType::PushBack) {
                            std::visit([&](auto &&v) { _S.Set(id, _S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).push_back(std::move(v))); }, *op.Value);
                        } else if (op.Op == PatchOpType::Set) {
                            std::visit([&](auto &&v) { _S.Set(id, _S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).set(*op.Index, std::move(v))); }, *op.Value);
                        } else {
                            // `set` ops - currently, u32 is the only set value type.
                            std::visit(
                                Match{
                                    [&](u32 v) {
                                        if (op.Op == PatchOpType::Insert) _S.Set(id, _S.Get<immer::set<decltype(v)>>(id).insert(v));
                                        else if (op.Op == PatchOpType::Erase) _S.Set(id, _S.Get<immer::set<decltype(v)>>(id).erase(v));
                                    },
                                    [](auto &&) {},
                                },
                                *op.Value
                            );
                        }
                    }
                }
            },
            [this](ProjectCore::ActionType &&a) { Core.Apply(std::move(a)); },
            [this](AppActionType &&a) { App->Apply(std::move(a)); },
        },
        action
    );
}

bool Project::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [](const Action::Project::OpenEmpty &) { return true; },
            [](const Action::Project::Open &a) { return fs::exists(a.file_path); },
            [](const Action::Project::OpenDefault &) { return fs::exists(DefaultProjectPath); },
            [](const Action::Project::ShowOpenDialog &) { return true; },
            [this](const Action::Project::ShowSaveDialog &) { return ProjectHasChanges; },
            [this](const Action::Project::Undo &) { return !ActiveGestureActions.empty() || History.CanUndo(); },
            [this](const Action::Project::Redo &) { return History.CanRedo(); },
            [this](const Action::Project::SetHistoryIndex &a) { return a.index < History.Size(); },
            [this](const Action::Project::Save &) { return !History.Empty(); },
            [this](const Action::Project::SaveDefault &) { return !History.Empty(); },
            [this](const Action::Project::SaveCurrent &) { return ProjectHasChanges; },
            [this](const Action::FileDialog::Open &) { return !FileDialog.Visible; },
            [](const Action::FileDialog::Select &) { return true; },
            [this](const Action::Core::Any &a) { return CoreHandler.CanApply(a); },
            [](const Action::Store::ApplyPatch &) { return true; },
            [this](const ProjectCore::ActionType &a) { return Core.CanApply(a); },
            [this](const AppActionType &a) { return App->CanApply(a); },
        },
        action
    );
}

using namespace ImGui;

static bool IsUserProjectPath(const fs::path &path) {
    return fs::relative(path).string() != fs::relative(EmptyProjectPath).string() &&
        fs::relative(path).string() != fs::relative(DefaultProjectPath).string();
}

void Project::SetCurrentProjectPath(const fs::path &path) const {
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
    IsWidgetGesturing = false;
    History.Clear(S);
    ClearChanged();
    LatestChangedPaths.clear();

    // When loading a new project, we always refresh all UI contexts.
    Core.Style.ImGui.IsChanged = true;
    Core.Style.ImPlot.IsChanged = true;
    ImGuiSettings::IsChanged = true;

    // Keep the canonical "empty" project up-to-date.
    if (!fs::exists(InternalPath)) fs::create_directory(InternalPath);
    Save(EmptyProjectPath);
}

// Create a patch comparing the current transient store with the current persistent store.
// **Resets the transient store to the current persisent store.**
Patch CreatePatchAndResetTransient(const PersistentStore &persistent, TransientStore &transient, ID base_id) {
    const auto patch = CreatePatch(persistent, transient.Persistent(), base_id);
    transient = persistent.Transient();
    return patch;
}

void Project::Tick() {
    auto &io = ImGui::GetIO();
    if (io.WantSaveIniSettings) {
        ImGui::SaveIniSettingsToMemory(); // Populate ImGui's `Settings...` context members.
        auto &imgui_settings = Core.ImGuiSettings;
        imgui_settings.Set(ImGui::GetCurrentContext());
        if (auto patch = CreatePatchAndResetTransient(S, _S, imgui_settings.Id); !patch.Empty()) {
            Q(Action::Store::ApplyPatch{std::move(patch)});
        }
        io.WantSaveIniSettings = false;
    }
    ApplyQueuedActions(false);
}

static json ReadFileJson(const fs::path &file_path) { return json::parse(FileIO::read(file_path)); }

// Helper function used in `Project::Open`.
// Modifies the active transient store.
void Project::OpenStateFormatProject(const fs::path &file_path) const {
    auto j = ReadFileJson(file_path);
    // First, refresh all component containers to ensure the dynamically managed component instances match the JSON.
    for (const ID auxiliary_id : Component::ContainerAuxiliaryIds) {
        if (auto *auxiliary_field = Component::ById.at(auxiliary_id); j.contains(auxiliary_field->JsonPointer())) {
            auxiliary_field->SetJson(std::move(j.at(auxiliary_field->JsonPointer())));
            auxiliary_field->Refresh();
            auxiliary_field->Parent->Refresh();
        }
    }

    // Now, every flattened JSON pointer is 1:1 with an instance path.
    State.SetJson(std::move(j));

    // We could do `RefreshChanged(_S.CheckedCommit(Id))`, and only refresh the changed components,
    // but this gets tricky with component containers, since the store patch will contain added/removed paths
    // that have already been accounted for above.
    S = _S.Persistent();
    ClearChanged();
    LatestChangedPaths.clear();
    for (auto *child : State.Children) child->Refresh();

    // Always update the ImGui context, regardless of the patch, to avoid expensive sifting through paths and just to be safe.
    Core.ImGuiSettings.IsChanged = true;
    History.Clear(S);
}

void Project::Open(const fs::path &file_path) const {
    const auto format = GetProjectFormat(file_path);
    if (!format) return; // TODO log

    IsWidgetGesturing = false;

    if (format == ProjectFormat::State) {
        OpenStateFormatProject(file_path);
    } else if (format == ProjectFormat::Action) {
        OpenStateFormatProject(EmptyProjectPath);

        IndexedGestures indexed_gestures = ReadFileJson(file_path);
        for (auto &&gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture.Actions) {
                std::visit(Match{[this](const Project::ActionType &a) { Apply(a); }}, action_moment.Action);
                RefreshChanged(CheckedCommit(State.Id));
            }
            History.AddGesture(S, std::move(gesture), State.Id);
        }
        SetHistoryIndex(indexed_gestures.Index);
        LatestChangedPaths.clear();
    }

    SetCurrentProjectPath(file_path);
}

float Project::GestureTimeRemainingSec() const {
    if (ActiveGestureActions.empty()) return 0;

    const float gesture_duration_sec = Core.Settings.GestureDurationSec;
    return std::max(0.f, gesture_duration_sec - fsec(Clock::now() - ActiveGestureActions.back().QueueTime).count());
}

Plottable Project::PathChangeFrequencyPlottable() const {
    if (History.GetChangedPathsCount() == 0 && GestureChangedPaths.empty()) return {};

    std::map<StorePath, u32> gesture_change_counts;
    for (const auto &[id, changed_paths] : GestureChangedPaths) {
        const auto &component = Component::ById.at(id);
        for (const auto &paths_moment : changed_paths) {
            for (const auto &path : paths_moment.second) {
                gesture_change_counts[path == "" ? component->Path : component->Path / path]++;
            }
        }
    }

    const auto history_change_counts = History.GetChangeCountById() | transform([](const auto &entry) { return std::pair(Component::ById.at(entry.first)->Path, entry.second); }) | to<std::map>();
    std::set<StorePath> paths;
    paths.insert_range(keys(history_change_counts));
    paths.insert_range(keys(gesture_change_counts));

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
        paths | transform([](const string &path) { return path.substr(1); }) | to<std::vector>(),
        values,
    };
}

void Project::OpenRecentProjectMenuItem() const {
    if (BeginMenu("Open recent project", !Preferences.RecentlyOpenedPaths.empty())) {
        for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
            if (MenuItem(recently_opened_path.filename().c_str())) Q(Action::Project::Open{recently_opened_path});
        }
        EndMenu();
    }
}

void Project::UpdateWidgetGesturing() const {
    if (ImGui::IsItemActivated()) IsWidgetGesturing = true;
    if (ImGui::IsItemDeactivated()) IsWidgetGesturing = false;
}

std::optional<TimePoint> Project::LatestUpdateTime(ID id, std::optional<StorePath> relative_path) noexcept {
    if (!LatestChangedPaths.contains(id)) return {};

    const auto &[update_time, paths] = LatestChangedPaths.at(id);
    if (!relative_path) return update_time;
    if (paths.contains(*relative_path)) return update_time;
    return {};
}

void Project::RenderPathChangeFrequency() const {
    auto [labels, values] = PathChangeFrequencyPlottable();
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
        const auto c_labels = labels | transform([](const std::string &label) { return label.c_str(); }) | to<std::vector>();
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), c_labels.data(), false);

        static const char *const ItemLabels[] = {"Committed updates", "Active updates"};
        const int item_count = HasGestureActions() ? 2 : 1;
        const int group_count = values.size() / item_count;
        ImPlot::PlotBarGroups(ItemLabels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

bool IsPressed(ImGuiKeyChord chord) {
    return IsKeyChordPressed(chord, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);
}

std::optional<Action::Project::Any> ProduceKeyboardAction() {
    using namespace Action::Project;

    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_N)) return OpenEmpty{};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_O)) return ShowOpenDialog{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_S)) return ShowSaveDialog{};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) return Undo{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_Z)) return Redo{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_O)) return OpenDefault{};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_S)) return SaveCurrent{};

    return {};
}

void Project::Draw() const {
    static const ActionMenuItem<ActionType>
        OpenEmptyMenuItem{*this, Action::Project::OpenEmpty{}, "Cmd+N"},
        ShowOpenDialogMenuItem{*this, Action::Project::ShowOpenDialog{}, "Cmd+O"},
        OpenDefaultMenuItem{*this, Action::Project::OpenDefault{}, "Shift+Cmd+O"},
        SaveCurrentMenuItem{*this, Action::Project::SaveCurrent{}, "Cmd+S"},
        SaveDefaultMenuItem{*this, Action::Project::SaveDefault{}},
        UndoMenuItem{*this, Action::Project::Undo{}, "Cmd+Z"},
        RedoMenuItem{*this, Action::Project::Redo{}, "Shift+Cmd+Z"};

    static const Menu MainMenu{
        {
            Menu(
                "File",
                {
                    OpenEmptyMenuItem,
                    ShowOpenDialogMenuItem,
                    [this]() { OpenRecentProjectMenuItem(); },
                    OpenDefaultMenuItem,
                    SaveCurrentMenuItem,
                    SaveDefaultMenuItem,
                }
            ),
            Menu(
                "Edit",
                {
                    UndoMenuItem,
                    RedoMenuItem,
                }
            ),
            [this] {
                if (BeginMenu("Windows")) {
                    State.DrawWindowsMenu();
                    EndMenu();
                }
            },
        },
        true
    };

    MainMenu.Draw();
    {
        auto dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
        if (ImGui::GetFrameCount() == 1) State.Dock(&dockspace_id);

        const auto &windows = Core.Windows;
        for (const auto *child : Core.Children) {
            if (!windows.IsWindow(child->Id) && child != &windows) child->Draw();
        }
        windows.Draw();

        if (ImGui::GetFrameCount() == 1) State.FocusDefault(); // todo default focus no longer working
    }

    FileDialog.Render();
    if (PrevSelectedPath != FileDialog.SelectedFilePath && FileDialog.Data.OwnerId == State.Id) {
        const fs::path selected_path = FileDialog.SelectedFilePath;
        PrevSelectedPath = FileDialog.SelectedFilePath = "";
        if (FileDialog.Data.SaveMode) Q(Action::Project::Save{selected_path});
        else Q(Action::Project::Open{selected_path});
    }
    if (auto action = ProduceKeyboardAction()) {
        std::visit([this](auto &&a) { Q(std::move(a)); }, *action);
    }
}

void ShowActions(const SavedActionMoments &actions) {
    for (u32 action_index = 0; action_index < actions.size(); action_index++) {
        const auto &[action, queue_time] = actions[action_index];
        if (TreeNodeEx(std::to_string(action_index).c_str(), ImGuiTreeNodeFlags_None, "%s", action.GetPath().string().c_str())) {
            BulletText("Queue time: %s", std::format("{:%Y-%m-%d %T}", queue_time).c_str());
            SameLine();
            flowgrid::HelpMarker("The original queue time of the action. If this is a merged action, this is the queue time of the most recent action in the merge.");
            auto data = json(action)[1];
            if (!data.is_null()) {
                SetNextItemOpen(true);
                flowgrid::JsonTree("Data", std::move(data));
            }
            TreePop();
        }
    }
}

void Project::RenderMetrics() const {
    {
        // Active (uncompressed) gesture
        if (const bool is_gesturing = IsWidgetGesturing, has_gesture_actions = HasGestureActions();
            is_gesturing || has_gesture_actions) {
            // Gesture completion progress bar (full-width to empty).
            const float time_remaining_sec = GestureTimeRemainingSec();
            const ImVec2 row_min{GetWindowPos().x, GetCursorScreenPos().y};
            const float gesture_ratio = time_remaining_sec / float(Core.Settings.GestureDurationSec);
            const ImRect gesture_ratio_rect{row_min, row_min + ImVec2{GetWindowWidth() * std::clamp(gesture_ratio, 0.f, 1.f), GetFontSize()}};
            GetWindowDrawList()->AddRectFilled(gesture_ratio_rect.Min, gesture_ratio_rect.Max, Core.Style.Project.Colors[ProjectCol_GestureIndicator]);

            const string active_gesture_title = std::format("Active gesture{}", has_gesture_actions ? " (uncompressed)" : "");
            if (TreeNodeEx(active_gesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (is_gesturing) FillRowItemBg(Core.Style.ImGui.Colors[ImGuiCol_FrameBgActive]);
                else BeginDisabled();
                Text("Widget gesture: %s", is_gesturing ? "true" : "false");
                if (!is_gesturing) EndDisabled();

                if (has_gesture_actions) ShowActions(GetGestureActions());
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
        const auto &history = History;
        const bool no_history = history.Empty();
        if (no_history) BeginDisabled();
        if (TreeNodeEx("History", ImGuiTreeNodeFlags_DefaultOpen, "History (Records: %d, Current record index: %d)", history.Size() - 1, history.Index)) {
            if (!no_history) {
                if (u32 edited_history_index = history.Index; SliderU32("History index", &edited_history_index, 0, history.Size() - 1)) {
                    Q(Action::Project::SetHistoryIndex{edited_history_index});
                }
            }
            for (u32 i = 1; i < history.Size(); i++) {
                // todo button to navitate to this history index.
                if (TreeNodeEx(std::to_string(i).c_str(), i == history.Index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &[store_record, gesture] = history.At(i);
                    BulletText("Gesture committed: %s\n", std::format("{:%Y-%m-%d %T}", gesture.CommitTime).c_str());
                    if (TreeNode("Actions")) {
                        ShowActions(gesture.Actions);
                        TreePop();
                    }
                    if (TreeNode("Patch")) {
                        // We compute patches as we need them rather than memoizing.
                        const auto &patch = CreatePatch(history.PrevStore(), history.CurrentStore(), State.Id);
                        for (const auto &[id, ops] : patch.Ops) {
                            const auto &path = Component::ById.at(id)->Path;
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
            Core.Debug.Metrics.Project.ShowRelativePaths.Draw();

            if (!has_RecentlyOpenedPaths) BeginDisabled();
            if (TreeNodeEx("Recently opened paths", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
                    BulletText("%s", (Core.Debug.Metrics.Project.ShowRelativePaths ? fs::relative(recently_opened_path) : recently_opened_path).c_str());
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
        flowgrid::HelpMarker(
            "All actions are internally stored in a `std::variant`, which must be large enough to hold its largest type. "
            "Thus, it's important to keep action data minimal."
        );
    }
}

void Project::ApplyQueuedActions(bool force_commit_gesture) {
    const bool has_gesture_actions = HasGestureActions();
    while (Queue.try_dequeue(DequeueToken, DequeueActionMoment)) {
        auto &[action, queue_time] = DequeueActionMoment;
        if (!CanApply(action)) continue;

        // Special cases:
        // * All actions except store patches are no-ops while the file dialog is open.
        //   - Store patches are allowed because they may include ImGui settings changes belonging to the file dialog.
        //   - TODO a better approach would be to exclude the filedialog window settings and everything belonging to it from the saved ImGuiSettings.
        //     As is, we try to restore saved file dialog window settings even when the file dialog is not open.
        if (FileDialog.Visible && !std::holds_alternative<Action::Store::ApplyPatch>(action)) {
            continue;
        }
        // * If saving the current project where there is none, open the save project dialog so the user can choose the save file:
        if (std::holds_alternative<Action::Project::SaveCurrent>(action) && !CurrentProjectPath) action = Action::Project::ShowSaveDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        // todo this should be an action option
        force_commit_gesture |=
            std::holds_alternative<Action::Primitive::Bool::Toggle>(action) ||
            std::holds_alternative<Action::Vec2::ToggleLinked>(action) ||
            std::holds_alternative<Action::AdjacencyList::ToggleConnection>(action) ||
            std::holds_alternative<Action::FileDialog::Select>(action);

        Apply(action);

        std::visit(
            Match{
                [this, &queue_time](const Action::Saved &a) {
                    if (auto patch = CheckedCommit(State.Id); !patch.Empty()) {
                        RefreshChanged(std::move(patch), true);
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

    if (force_commit_gesture || (!IsWidgetGesturing && has_gesture_actions && GestureTimeRemainingSec() <= 0)) {
        CommitGesture();
    }
}
