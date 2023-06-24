#include "Field.h"

#include "Core/Store/Patch/Patch.h"

#include "imgui.h"

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    FieldById.emplace(Id, this);
    FieldIdByPath.emplace(Path, Id);
}
Field::~Field() {
    FieldIdByPath.erase(Path);
    FieldById.erase(Id);
}

Field *Field::FindByPath(const StorePath &search_path) {
    if (FieldIdByPath.contains(search_path)) return FieldById[FieldIdByPath[search_path]];
    // Handle container fields.
    if (FieldIdByPath.contains(search_path.parent_path())) return FieldById[FieldIdByPath[search_path.parent_path()]];
    if (FieldIdByPath.contains(search_path.parent_path().parent_path())) return FieldById[FieldIdByPath[search_path.parent_path().parent_path()]];
    return nullptr;
}

void Field::FindAndMarkChanged(const Patch &patch) {
    for (const auto &path : patch.GetPaths()) {
        const auto *changed_field = FindByPath(path);
        if (changed_field == nullptr) throw std::runtime_error(std::format("Patch affects a path belonging to an unknown field: {}", path.string()));

        ChangedFieldIds.insert(changed_field->Id);
    }
}

void Field::RefreshChanged(const Patch &patch) {
    FindAndMarkChanged(patch);
    static std::unordered_set<ChangeListener *> AffectedListeners;
    for (const auto changed_field_id : ChangedFieldIds) {
        auto *changed_field = FieldById[changed_field_id];
        changed_field->RefreshValue();
        const auto &listeners = ChangeListenersForField[changed_field_id];
        AffectedListeners.insert(listeners.begin(), listeners.end());
    }

    for (auto *listener : AffectedListeners) listener->OnFieldChanged();
    AffectedListeners.clear();
    ChangedFieldIds.clear();
}

void Field::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}
