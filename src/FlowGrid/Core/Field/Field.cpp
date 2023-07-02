#include "Field.h"

#include "imgui.h"

#include "Core/Store/Patch/Patch.h"
#include "App/Style/Style.h"

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    FieldById.emplace(Id, this);
    FieldIdByPath.emplace(Path, Id);
}
Field::~Field() {
    FieldIdByPath.erase(Path);
    FieldById.erase(Id);
}

void Field::FindAndMarkChanged(const Patch &patch) {
    ClearChanged();
    for (const auto &path : patch.GetPaths()) {
        const auto *changed_field = FindByPath(path);
        if (changed_field == nullptr) throw std::runtime_error(std::format("Patch affects a path belonging to an unknown field: {}", path.string()));

        const auto relative_path = path == changed_field->Path ? fs::path("") : path.lexically_relative(changed_field->Path);
        ChangedPathsByFieldId[changed_field->Id].insert(relative_path);

        // Mark all ancestor components as changed.
        const Component *ancestor = changed_field;
        while (ancestor != nullptr) {
            ChangedComponentIds.insert(ancestor->Id);
            ancestor = ancestor->Parent;
        }
    }
}

void Field::RefreshChanged(const Patch &patch) {
    FindAndMarkChanged(patch);
    static std::unordered_set<ChangeListener *> affected_listeners;
    for (const auto &[field_id, _] : ChangedPathsByFieldId) {
        auto *changed_field = FieldById[field_id];
        changed_field->RefreshValue();
        const auto &listeners = ChangeListenersByFieldId[field_id];
        affected_listeners.insert(listeners.begin(), listeners.end());
    }

    for (auto *listener : affected_listeners) listener->OnFieldChanged();
    affected_listeners.clear();
}
void Field::RefreshAll() {
    for (auto &[id, field] : FieldById) field->RefreshValue();
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
    if (!GestureChangedPathsByFieldId.contains(component_id)) {
        // const auto *field = ById[component_id];
        // if (!History.CommitTimesByPath.contains(field->Path)) return {}; // No commits yet.
        // return History.CommitTimesByPath[field->Path].back();
        return {};
    }

    return GestureChangedPathsByFieldId.at(component_id).back().first;
}

void Field::RenderValueTree(ValueTreeLabelMode mode, bool auto_select) const {
    // Flash background color of changed fields.
    if (const auto latest_update_time = LatestUpdateTime(Id)) {
        const float flash_elapsed_ratio = fsec(Clock::now() - *latest_update_time).count() / fg::style.FlowGrid.FlashDurationSec;
        ImColor flash_color = fg::style.FlowGrid.Colors[FlowGridCol_Flash];
        flash_color.Value.w = std::max(0.f, 1 - flash_elapsed_ratio);
        FillRowItemBg(flash_color);
    }
}
