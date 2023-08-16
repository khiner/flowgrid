#include "Field.h"

#include "imgui.h"

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

Field *Field::FindChanged(const StorePath &path, PatchOp::Type op) {
    if ((op == PatchOp::Add || op == PatchOp::Remove) && !StringHelper::IsInteger(path.filename().string())) {
        // Do not mark any fields as added/removed if they are within a component container.
        // The container's auxiliary field is marked as changed instead (and its path will be in same patch).
        if (auto *component_container = FindComponentContainerFieldByPath(path)) return nullptr;
    }
    auto *field = Find(path);
    if (field && ComponentContainerAuxiliaryFields.contains(field->Id)) {
        // When a container's auxiliary field is changed, mark the container as changed instead.
        return static_cast<Field *>(field->Parent);
    }

    if (!field) throw std::runtime_error(std::format("Could not find a field to attribute for op: {} at path: {}", to_string(op), path.string()));

    return field;
}

void Field::MarkAllChanged(const Patch &patch) {
    const auto change_time = Clock::now();
    ClearChanged();

    for (const auto &[partial_path, op] : patch.Ops) {
        const auto path = patch.BasePath / partial_path;
        if (auto *changed_field = FindChanged(path, op.Op)) {
            const ID id = changed_field->Id;
            const StorePath relative_path = path == changed_field->Path ? "" : path.lexically_relative(changed_field->Path);
            ChangedPaths[id].first = change_time;
            ChangedPaths[id].second.insert(relative_path);

            // Mark the changed field and all its ancestors.
            ChangedFieldIds.insert(id);
            const Component *ancestor = changed_field->Parent;
            while (ancestor != nullptr) {
                ChangedAncestorComponentIds.insert(ancestor->Id);
                ancestor = ancestor->Parent;
            }
        }
    }

    // Copy `ChangedPaths` over to `LatestChangedPaths`.
    // (`ChangedPaths` is cleared at the end of each action, while `LatestChangedPaths` is retained for the lifetime of the application.)
    for (const auto &[field_id, paths_moment] : ChangedPaths) LatestChangedPaths[field_id] = paths_moment;
}

void Field::RefreshChanged(const Patch &patch, bool add_to_gesture) {
    MarkAllChanged(patch);
    static std::unordered_set<ChangeListener *> affected_listeners;

    // Find field listeners to notify.
    for (const auto changed_id : ChangedFieldIds) {
        if (!FieldById.contains(changed_id)) continue; // The field was deleted.

        auto *changed_field = FieldById.at(changed_id);
        changed_field->Refresh();

        const auto &listeners = ChangeListenersByFieldId[changed_id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    // Find ancestor listeners to notify.
    // (Listeners can disambiguate by checking `IsChanged(bool include_descendents = false)` and `IsDescendentChanged()`.)
    for (const auto changed_id : ChangedAncestorComponentIds) {
        if (!ById.contains(changed_id)) continue; // The component was deleted.

        const auto &listeners = ChangeListenersByFieldId[changed_id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    for (auto *listener : affected_listeners) listener->OnFieldChanged();
    affected_listeners.clear();

    // Update gesture paths.
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

void Field::RenderValueTree(bool annotate, bool auto_select) const {
    // Flash background color of changed fields.
    if (const auto latest_update_time = LatestUpdateTime(Id)) {
        const float flash_elapsed_ratio = fsec(Clock::now() - *latest_update_time).count() / fg::style.FlowGrid.FlashDurationSec;
        ImColor flash_color = fg::style.FlowGrid.Colors[FlowGridCol_Flash];
        flash_color.Value.w = std::max(0.f, 1 - flash_elapsed_ratio);
        FillRowItemBg(flash_color);
    }
}
