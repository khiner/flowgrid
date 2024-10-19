
#include "Project.h"

#include "imgui_internal.h"

#include <format>
#include <ranges>
#include <set>

#include "Application/ApplicationPreferences.h"

#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/ActionQueue.h"
#include "Core/Store/Store.h"
#include "Core/Store/StoreHistory.h"

#include "Helper/File.h"
#include "Helper/String.h"
#include "Helper/Time.h"

using namespace FlowGrid;

using std::ranges::to, std::views::join, std::views::keys, std::views::transform;

// Project constants:
static const fs::path InternalPath = ".flowgrid";
// Order matters here, as the first extension is the default project extension.
static const std::map<ProjectFormat, std::string_view> ExtensionByProjectFormat{
    {ProjectFormat::Action, ".fla"},
    {ProjectFormat::State, ".fls"},
};
static const auto ProjectFormatByExtension = ExtensionByProjectFormat | transform([](const auto &pe) { return std::pair(pe.second, pe.first); }) | to<std::map>();
static const auto AllProjectExtensions = ProjectFormatByExtension | keys;
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

Project::Project(Store &store, moodycamel::ConsumerToken ctok, ActionableProducer::EnqueueFn q)
    : ActionableProducer(std::move(q)), S(store), _S(store), State(store, q, ProjectContext),
      HistoryPtr(std::make_unique<StoreHistory>(store)), History(*HistoryPtr),
      DequeueToken(std::make_unique<moodycamel::ConsumerToken>(std::move(ctok))) {}

Project::~Project() = default;

void Project::RefreshChanged(Patch &&patch, bool add_to_gesture) const {
    MarkAllChanged(std::move(patch));

    std::unordered_set<Component::ChangeListener *> affected_listeners;

    // Find listeners to notify.
    for (const auto id : Component::ChangedIds) {
        if (!Component::ById.contains(id)) continue; // The component was deleted.

        Component::ById.at(id)->Refresh();

        const auto &listeners = Component::ChangeListenersById[id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    // Find ancestor listeners to notify.
    // (Listeners can disambiguate by checking `IsChanged(bool include_descendents = false)` and `IsDescendentChanged()`.)
    for (const auto id : Component::ChangedAncestorComponentIds) {
        if (!Component::ById.contains(id)) continue; // The component was deleted.

        const auto &listeners = Component::ChangeListenersById[id];
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
    Component::ChangedIds.clear();
    Component::ChangedAncestorComponentIds.clear();
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
            Component::ChangedIds.insert(id);
            for (const auto *ancestor = changed->Parent; ancestor != nullptr; ancestor = ancestor->Parent) {
                Component::ChangedAncestorComponentIds.insert(ancestor->Id);
            }
        }
    }

    // Copy `ChangedPaths` over to `LatestChangedPaths`.
    // (`ChangedPaths` is cleared at the end of each action, while `LatestChangedPaths` is retained for the lifetime of the application.)
    for (const auto &[field_id, paths_moment] : ChangedPaths) Component::LatestChangedPaths[field_id] = paths_moment;
}

void Project::CommitGesture() const {
    GestureChangedPaths.clear();
    if (ActiveGestureActions.empty()) return;

    const auto merged_actions = MergeActions(ActiveGestureActions);
    ActiveGestureActions.clear();
    if (merged_actions.empty()) return;

    AddGesture({merged_actions, Clock::now()});
}

void Project::AddGesture(Gesture &&gesture) const { History.AddGesture(S, std::move(gesture), State.Id); }

void Project::SetHistoryIndex(u32 index) const {
    if (index == History.Index) return;

    GestureChangedPaths.clear();
    ActiveGestureActions.clear(); // In case we're mid-gesture, revert before navigating.
    History.SetIndex(index);
    const auto &store = History.CurrentStore();
    auto patch = _S.CreatePatch(store, State.Id);
    _S.Commit(store.Maps);
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

json Project::GetProjectJson(const ProjectFormat format) const {
    switch (format) {
        case ProjectFormat::State: return State.ToJson();
        case ProjectFormat::Action: return History.GetIndexedGestures();
    }
}

void ApplyVectorSet(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::flex_vector<decltype(a.value)>>(a.component_id).set(a.i, a.value));
}
void ApplySetInsert(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::set<decltype(a.value)>>(a.component_id).insert(a.value));
}
void ApplySetErase(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::set<decltype(a.value)>>(a.component_id).erase(a.value));
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
                State.FileDialog.Set({
                    .OwnerId = State.Id,
                    .Title = "Choose file",
                    .Filters = AllProjectExtensionsDelimited,
                });
            },
            [this](const Action::Project::ShowSaveDialog &) { State.FileDialog.Set({State.Id, "Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}); },
            /* File dialog */
            [this](const Action::FileDialog::Open &a) { State.FileDialog.SetJson(json::parse(a.dialog_json)); },
            // `SelectedFilePath` mutations are non-stateful side effects.
            [this](const Action::FileDialog::Select &a) { State.FileDialog.SelectedFilePath = a.file_path; },
            /* Primitives */
            [this](const Action::Primitive::Bool::Toggle &a) { _S.Set(a.component_id, !S.Get<bool>(a.component_id)); },
            [this](const Action::Primitive::Int::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::UInt::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Float::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Enum::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Flags::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::String::Set &a) { _S.Set(a.component_id, a.value); },
            /* Containers */
            [this](const Action::Container::Any &a) {
                const auto *container = Component::ById.at(a.GetComponentId());
                std::visit(
                    Match{
                        [container](const Action::AdjacencyList::ToggleConnection &a) {
                            const auto *al = static_cast<const AdjacencyList *>(container);
                            if (al->IsConnected(a.source, a.destination)) al->Disconnect(a.source, a.destination);
                            else al->Connect(a.source, a.destination);
                        },
                        [this, container](const Action::Vec2::Set &a) {
                            const auto *vec2 = static_cast<const Vec2 *>(container);
                            _S.Set(vec2->X.Id, a.value.first);
                            _S.Set(vec2->Y.Id, a.value.second);
                        },
                        [this, container](const Action::Vec2::SetX &a) { _S.Set(static_cast<const Vec2 *>(container)->X.Id, a.value); },
                        [this, container](const Action::Vec2::SetY &a) { _S.Set(static_cast<const Vec2 *>(container)->Y.Id, a.value); },
                        [this, container](const Action::Vec2::SetAll &a) {
                            const auto *vec2 = static_cast<const Vec2 *>(container);
                            _S.Set(vec2->X.Id, a.value);
                            _S.Set(vec2->Y.Id, a.value);
                        },
                        [this, container](const Action::Vec2::ToggleLinked &) {
                            const auto *vec2 = static_cast<const Vec2Linked *>(container);
                            _S.Set(vec2->Linked.Id, !S.Get<bool>(vec2->Linked.Id));
                            const float x = S.Get<float>(vec2->X.Id);
                            const float y = S.Get<float>(vec2->Y.Id);
                            if (x < y) _S.Set(vec2->Y.Id, x);
                            else if (y < x) _S.Set(vec2->X.Id, y);
                        },
                        [this](const Action::Vector<bool>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<int>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<u32>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<float>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<std::string>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Set<u32>::Insert &a) { ApplySetInsert(_S, a); },
                        [this](const Action::Set<u32>::Erase &a) { ApplySetErase(_S, a); },
                        [this, container](const Action::Navigable<u32>::Clear &) {
                            const auto *nav = static_cast<const Navigable<u32> *>(container);
                            _S.Set<immer::flex_vector<u32>>(nav->Value.Id, {});
                            _S.Set(nav->Cursor.Id, 0);
                        },
                        [this, container](const Action::Navigable<u32>::Push &a) {
                            const auto *nav = static_cast<const Navigable<u32> *>(container);
                            const auto vec = S.Get<immer::flex_vector<u32>>(nav->Value.Id).push_back(a.value);
                            _S.Set<immer::flex_vector<u32>>(nav->Value.Id, vec);
                            _S.Set<u32>(nav->Cursor.Id, vec.size() - 1);
                        },

                        [this, container](const Action::Navigable<u32>::MoveTo &a) {
                            const auto *nav = static_cast<const Navigable<u32> *>(container);
                            auto cursor = u32(std::clamp(int(a.index), 0, int(S.Get<immer::flex_vector<u32>>(nav->Value.Id).size()) - 1));
                            _S.Set(nav->Cursor.Id, std::move(cursor));
                        },
                    },
                    a
                );
            },
            /* Store */
            [this](const Action::Store::ApplyPatch &a) {
                for (const auto &[id, ops] : a.patch.Ops) {
                    for (const auto &op : ops) {
                        if (op.Op == PatchOpType::PopBack) {
                            std::visit(
                                [&](auto &&v) {
                                    const auto vec = S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id);
                                    _S.Set(id, vec.take(vec.size() - 1));
                                },
                                *op.Old
                            );
                        } else if (op.Op == PatchOpType::Remove) {
                            std::visit([&](auto &&v) { _S.Erase<std::decay_t<decltype(v)>>(id); }, *op.Old);
                        } else if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) {
                            std::visit([&](auto &&v) { _S.Set(id, std::move(v)); }, *op.Value);
                        } else if (op.Op == PatchOpType::PushBack) {
                            std::visit([&](auto &&v) { _S.Set(id, S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).push_back(std::move(v))); }, *op.Value);
                        } else if (op.Op == PatchOpType::Set) {
                            std::visit([&](auto &&v) { _S.Set(id, S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).set(*op.Index, std::move(v))); }, *op.Value);
                        } else {
                            // `set` ops - currently, u32 is the only set value type.
                            std::visit(
                                Match{
                                    [&](u32 v) {
                                        if (op.Op == PatchOpType::Insert) _S.Set(id, S.Get<immer::set<decltype(v)>>(id).insert(v));
                                        else if (op.Op == PatchOpType::Erase) _S.Set(id, S.Get<immer::set<decltype(v)>>(id).erase(v));
                                    },
                                    [](auto &&) {},
                                },
                                *op.Value
                            );
                        }
                    }
                }
            },
            [this](Action::State::Any &&a) { State.Apply(std::move(a)); },
        },
        action
    );
}

bool Project::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](const Action::Project::Undo &) { return !ActiveGestureActions.empty() || History.CanUndo(); },
            [this](const Action::Project::Redo &) { return History.CanRedo(); },
            [this](const Action::Project::SetHistoryIndex &a) { return a.index < History.Size(); },
            [this](const Action::Project::Save &) { return !History.Empty(); },
            [this](const Action::Project::SaveDefault &) { return !History.Empty(); },
            [this](const Action::Project::ShowSaveDialog &) { return ProjectHasChanges; },
            [this](const Action::Project::SaveCurrent &) { return ProjectHasChanges; },
            [](const Action::Project::OpenDefault &) { return fs::exists(DefaultProjectPath); },
            [this](const Action::FileDialog::Open &) { return !State.FileDialog.Visible; },
            [this](Action::State::Any &&a) { return State.CanApply(std::move(a)); },
            [](auto &&) { return true; } // All other actions
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
    Component::IsWidgetGesturing = false;
    History.Clear(S);
    ClearChanged();
    Component::LatestChangedPaths.clear();

    // When loading a new project, we always refresh all UI contexts.
    State.Style.ImGui.IsChanged = true;
    State.Style.ImPlot.IsChanged = true;
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
    for (const ID auxiliary_id : Component::ContainerAuxiliaryIds) {
        if (auto *auxiliary_field = Component::ById.at(auxiliary_id); j.contains(auxiliary_field->JsonPointer())) {
            auxiliary_field->SetJson(std::move(j.at(auxiliary_field->JsonPointer())));
            auxiliary_field->Refresh();
            auxiliary_field->Parent->Refresh();
        }
    }

    // Now, every flattened JSON pointer is 1:1 with an instance path.
    State.SetJson(std::move(j));

    // We could do `RefreshChanged(S.CheckedCommit(Id))`, and only refresh the changed components,
    // but this gets tricky with component containers, since the store patch will contain added/removed paths
    // that have already been accounted for above.
    _S.Commit();
    ClearChanged();
    Component::LatestChangedPaths.clear();
    for (auto *child : State.Children) child->Refresh();

    // Always update the ImGui context, regardless of the patch, to avoid expensive sifting through paths and just to be safe.
    State.ImGuiSettings.IsChanged = true;
    History.Clear(S);
}

void Project::Open(const fs::path &file_path) const {
    const auto format = GetProjectFormat(file_path);
    if (!format) return; // TODO log

    Component::IsWidgetGesturing = false;

    if (format == ProjectFormat::State) {
        OpenStateFormatProject(file_path);
    } else if (format == ProjectFormat::Action) {
        OpenStateFormatProject(EmptyProjectPath);

        StoreHistory::IndexedGestures indexed_gestures = ReadFileJson(file_path);
        for (auto &&gesture : indexed_gestures.Gestures) {
            for (const auto &action_moment : gesture.Actions) {
                std::visit(Match{[this](const Project::ActionType &a) { Apply(a); }}, action_moment.Action);
                RefreshChanged(_S.CheckedCommit(State.Id));
            }
            AddGesture(std::move(gesture));
        }
        SetHistoryIndex(indexed_gestures.Index);
        Component::LatestChangedPaths.clear();
    }

    SetCurrentProjectPath(file_path);
}

float Project::GestureTimeRemainingSec() const {
    if (ActiveGestureActions.empty()) return 0;

    const float gesture_duration_sec = State.Settings.GestureDurationSec;
    return std::max(0.f, gesture_duration_sec - fsec(Clock::now() - ActiveGestureActions.back().QueueTime).count());
}

Plottable Project::StorePathChangeFrequencyPlottable() const {
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

void Project::WindowMenuItem() const {
    const auto &item = [this](const Component &c) {
        if (MenuItem(c.ImGuiLabel.c_str(), nullptr, State.Windows.IsVisible(c.Id))) {
            State.Q(Action::Windows::ToggleVisible{c.Id});
        }
    };
    if (BeginMenu("Windows")) {
        if (BeginMenu("Audio")) {
            item(State.Audio.Graph);
            item(State.Audio.Graph.Connections);
            item(State.Audio.Style);
            EndMenu();
        }
        if (BeginMenu("Faust")) {
            item(State.Audio.Faust.FaustDsps);
            item(State.Audio.Faust.Graphs);
            item(State.Audio.Faust.Paramss);
            item(State.Audio.Faust.Logs);
            EndMenu();
        }
        if (BeginMenu("Debug")) {
            item(State.Debug);
            item(State.Debug.StatePreview);
            item(State.Debug.StorePathUpdateFrequency);
            item(State.Debug.DebugLog);
            item(State.Debug.StackTool);
            item(State.Debug.Metrics);
            EndMenu();
        }
        item(State.Style);
        item(State.Demo);
        item(State.Info);
        item(State.Settings);
        EndMenu();
    }
}

#include "implot.h"

#include "UI/HelpMarker.h"
#include "UI/JsonTree.h"

void Project::RenderStorePathChangeFrequency() const {
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
        const auto c_labels = labels | transform([](const std::string &label) { return label.c_str(); }) | to<std::vector>();
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), c_labels.data(), false);

        static const char *ItemLabels[] = {"Committed updates", "Active updates"};
        const int item_count = HasGestureActions() ? 2 : 1;
        const int group_count = values.size() / item_count;
        ImPlot::PlotBarGroups(ItemLabels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

bool IsPressed(ImGuiKeyChord chord) {
    return IsKeyChordPressed(chord, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);
}

// todo return and handle `Action::Project::Any` subtype
std::optional<Action::Any> ProduceKeyboardAction() {
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
    MainMenu.Draw();
    State.Draw();
    if (PrevSelectedPath != State.FileDialog.SelectedFilePath && State.FileDialog.Data.OwnerId == State.Id) {
        const fs::path selected_path = State.FileDialog.SelectedFilePath;
        PrevSelectedPath = State.FileDialog.SelectedFilePath = "";
        if (State.FileDialog.Data.SaveMode) Q(Action::Project::Save{selected_path});
        else Q(Action::Project::Open{selected_path});
    }
    if (auto action = ProduceKeyboardAction()) Q(*action);
}

void ShowActions(const SavedActionMoments &actions) {
    for (u32 action_index = 0; action_index < actions.size(); action_index++) {
        const auto &[action, queue_time] = actions[action_index];
        if (TreeNodeEx(std::to_string(action_index).c_str(), ImGuiTreeNodeFlags_None, "%s", action.GetPath().string().c_str())) {
            BulletText("Queue time: %s", std::format("{:%Y-%m-%d %T}", queue_time).c_str());
            SameLine();
            fg::HelpMarker("The original queue time of the action. If this is a merged action, this is the queue time of the most recent action in the merge.");
            auto data = json(action)[1];
            if (!data.is_null()) {
                SetNextItemOpen(true);
                JsonTree("Data", std::move(data));
            }
            TreePop();
        }
    }
}

void Project::RenderMetrics() const {
    {
        // Active (uncompressed) gesture
        if (const bool is_gesturing = Component::IsWidgetGesturing, has_gesture_actions = HasGestureActions();
            is_gesturing || has_gesture_actions) {
            // Gesture completion progress bar (full-width to empty).
            const float time_remaining_sec = GestureTimeRemainingSec();
            const ImVec2 row_min{GetWindowPos().x, GetCursorScreenPos().y};
            const float gesture_ratio = time_remaining_sec / float(State.Settings.GestureDurationSec);
            const ImRect gesture_ratio_rect{row_min, row_min + ImVec2{GetWindowWidth() * std::clamp(gesture_ratio, 0.f, 1.f), GetFontSize()}};
            GetWindowDrawList()->AddRectFilled(gesture_ratio_rect.Min, gesture_ratio_rect.Max, State.Style.FlowGrid.Colors[FlowGridCol_GestureIndicator]);

            const string active_gesture_title = std::format("Active gesture{}", has_gesture_actions ? " (uncompressed)" : "");
            if (TreeNodeEx(active_gesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (is_gesturing) FillRowItemBg(State.Style.ImGui.Colors[ImGuiCol_FrameBgActive]);
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
                        const auto &patch = Store::CreatePatch(history.PrevStore().Maps, history.CurrentStore().Maps, State.Id);
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
            State.Debug.Metrics.FlowGrid.ShowRelativePaths.Draw();

            if (!has_RecentlyOpenedPaths) BeginDisabled();
            if (TreeNodeEx("Recently opened paths", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto &recently_opened_path : Preferences.RecentlyOpenedPaths) {
                    BulletText("%s", (State.Debug.Metrics.FlowGrid.ShowRelativePaths ? fs::relative(recently_opened_path) : recently_opened_path).c_str());
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

void Project::ApplyQueuedActions(ActionQueue<ActionType> &queue, bool force_commit_gesture) const {
    const bool has_gesture_actions = HasGestureActions();
    while (queue.TryDequeue(*DequeueToken.get(), DequeueActionMoment)) {
        auto &[action, queue_time] = DequeueActionMoment;
        if (!CanApply(action)) continue;

        // Special cases:
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
                [this, &store = _S, &queue_time](const Action::Saved &a) {
                    if (auto patch = store.CheckedCommit(State.Id); !patch.Empty()) {
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

    if (force_commit_gesture || (!Component::IsWidgetGesturing && has_gesture_actions && GestureTimeRemainingSec() <= 0)) {
        CommitGesture();
    }
}
