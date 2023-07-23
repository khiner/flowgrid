#include "Field.h"

#include "imgui.h"

#include "Core/Store/Patch/Patch.h"
#include "Project/Style/Style.h"

#include "Helper/String.h"

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    FieldById.emplace(Id, this);
    FieldIdByPath.emplace(Path, Id);
    Refresh();
}

Field::~Field() {
    Erase();
    FieldIdByPath.erase(Path);
    FieldById.erase(Id);
}

std::optional<std::filesystem::path> Field::FindLongestIntegerSuffixSubpath(const StorePath &p) {
    std::filesystem::path subpath = p;
    for (const auto &segment : std::views::reverse(p)) {
        if (StringHelper::IsInteger(segment.string())) return subpath;
        subpath = subpath.parent_path();
    }
    return std::nullopt; // No segment is an integer.
}

Field *Field::FindVectorFieldByChildPath(const StorePath &search_path) {
    const auto index_subpath = FindLongestIntegerSuffixSubpath(search_path);
    return index_subpath ? FindByPath(*index_subpath) : nullptr;
}

void Field::FindAndMarkChanged(const Patch &patch) {
    ClearChanged();
    const auto change_time = Clock::now();
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto path = patch.BasePath / partial_path;
        Field *affected_field;
        if (op.Op == PatchOp::Add || op.Op == PatchOp::Remove) {
            affected_field = FindVectorFieldByChildPath(path);
            // This add/remove could be within a non-vector container.
            // E.g. `AdjacencyList` is a container that stores its children directly under it, not under integer index subpaths.
            if (affected_field == nullptr) affected_field = FindByPath(path);
        } else {
            affected_field = FindByPath(path);
        }
        if (affected_field == nullptr) throw std::runtime_error(std::format("Patch affects a path belonging to an unknown field: {}", path.string()));

        const auto relative_path = path == affected_field->Path ? fs::path("") : path.lexically_relative(affected_field->Path);
        PathsMoment &paths_moment = ChangedPaths[affected_field->Id];
        paths_moment.first = change_time;
        paths_moment.second.insert(relative_path);

        // Mark the affected field all its ancestor components as changed.
        const Component *ancestor = affected_field;
        while (ancestor != nullptr) {
            ChangedComponentIds.insert(ancestor->Id);
            ancestor = ancestor->Parent;
        }
    }

    // Copy `ChangedPaths` over to `LatestChangedPaths`.
    // (`ChangedPaths` is cleared at the end of each action batch, while `LatestChangedPaths` is retained for the lifetime of the application.)
    for (const auto &[field_id, paths_moment] : ChangedPaths) LatestChangedPaths[field_id] = paths_moment;
}

void Field::RefreshChanged(const Patch &patch, bool add_to_gesture) {
    FindAndMarkChanged(patch);
    static std::unordered_set<ChangeListener *> affected_listeners;
    for (const auto &[field_id, _] : ChangedPaths) {
        auto *changed_field = FieldById[field_id];
        changed_field->Refresh();
        const auto &listeners = ChangeListenersByFieldId[field_id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    for (auto *listener : affected_listeners) listener->OnFieldChanged();
    affected_listeners.clear();
    if (add_to_gesture) {
        for (const auto &[field_id, paths_moment] : ChangedPaths) {
            GestureChangedPaths[field_id].push_back(paths_moment);
        }
    }
}
void Field::RefreshAll() {
    for (auto &[id, field] : FieldById) field->Refresh();
    std::unordered_set<ChangeListener *> all_listeners;
    for (auto &[id, listeners] : ChangeListenersByFieldId) {
        all_listeners.insert(listeners.begin(), listeners.end());
    }
}

void Field::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}

std::optional<TimePoint> Field::LatestUpdateTime(const ID component_id) {
    if (!LatestChangedPaths.contains(component_id)) return {};

    return LatestChangedPaths.at(component_id).first;
}

void Field::RenderValueTree(bool annotate, bool auto_select) const {
    // Flash background color of changed fields.
    if (const auto latest_update_time = LatestUpdateTime(Id)) {
        const float flash_elapsed_ratio = fsec(Clock::now() - *latest_update_time).count() / fg::style.FlowGrid.FlashDurationSec;
        ImColor flash_color = fg::style.FlowGrid.Colors[FlowGridCol_Flash];
        flash_color.Value.w = std::max(0.f, 1 - flash_elapsed_ratio);
        FillRowItemBg(flash_color);
    }
}
