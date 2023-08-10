#include "Field.h"

#include "imgui.h"

#include "Core/Store/Patch/Patch.h"
#include "Helper/String.h"
#include "Project/Style/Style.h"

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    FieldById.emplace(Id, this);
    FieldIdByPath.emplace(Path, Id);
    Refresh();
}

Field::~Field() {
    Erase();
    FieldIdByPath.erase(Path);
    FieldById.erase(Id);
    ChangeListenersByFieldId.erase(Id);
}

Field *Field::FindComponentContainerFieldByPath(const StorePath &search_path) {
    StorePath subpath = search_path;
    while (subpath != "/") {
        if (FieldIdByPath.contains(subpath)) {
            const auto field_id = FieldIdByPath.at(subpath);
            if (ComponentContainerFields.contains(field_id)) return FieldById[field_id];
        }
        subpath = subpath.parent_path();
    }
    return nullptr;
}

void Field::FindAndMarkChanged(const Patch &patch) {
    ClearChanged();
    const auto change_time = Clock::now();
    for (const auto &[partial_path, op] : patch.Ops) {
        const auto path = patch.BasePath / partial_path;
        Field *affected_field;
        if ((op.Op == PatchOp::Add || op.Op == PatchOp::Remove) && !StringHelper::IsInteger(path.filename().string())) {
            affected_field = FindComponentContainerFieldByPath(path); // Look for the nearest ancestor component container field.
            if (affected_field) continue; // The auxiliary field accounts for
            // This add/remove could be within a non-component container.
            // E.g. `AdjacencyList` is a container that stores its children directly under it, not under index subpaths.
        }
        affected_field = FindByPath(path);
        if (affected_field && ComponentContainerAuxiliaryFields.contains(affected_field->Id)) {
            // When a container's auxiliary field is changed, mark the container as changed instead.
            affected_field = static_cast<Field *>(affected_field->Parent);
        }
        if (affected_field == nullptr) throw std::runtime_error(std::format("Patch affects a path belonging to an unknown field: {}", path.string()));

        const auto relative_path = path == affected_field->Path ? fs::path("") : path.lexically_relative(affected_field->Path);
        PathsMoment &paths_moment = ChangedPaths[affected_field->Id];
        paths_moment.first = change_time;
        paths_moment.second.insert(relative_path);

        ChangedComponentIds.insert(affected_field->Id);
        // Mark all ancestor components of the affected field as changed.
        const Component *ancestor = affected_field->Parent;
        while (ancestor != nullptr) {
            ChangedAncestorComponentIds.insert(ancestor->Id);
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
    for (const auto changed_id : ChangedComponentIds) {
        if (!FieldById.contains(changed_id)) continue; // The field was deleted.

        auto *changed_field = FieldById.at(changed_id);
        changed_field->Refresh();

        const auto &listeners = ChangeListenersByFieldId[changed_id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    // Notify ancestors. (Listeners can disambiguate by checking `IsChanged()` vs `IsDescendentChanged()`.)
    for (const auto changed_id : ChangedAncestorComponentIds) {
        if (!ById.contains(changed_id)) continue; // The component was deleted.

        const auto &listeners = ChangeListenersByFieldId[changed_id];
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
