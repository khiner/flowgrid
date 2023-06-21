#include "Field.h"

#include "imgui.h"

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    Index = Instances.size();
    Instances.push_back(this);
    ValueIsStale.push_back(false);
    IndexForPath[Path] = Index;
}
Field::~Field() {
    IndexForPath.erase(Path);
    Instances.erase(Instances.begin() + Index);
    ValueIsStale.erase(ValueIsStale.begin() + Index);
}

Field *Field::FindByPath(const StorePath &search_path) {
    if (IndexForPath.contains(search_path)) return Instances[IndexForPath[search_path]];
    // Handle container fields.
    if (IndexForPath.contains(search_path.parent_path())) return Instances[IndexForPath[search_path.parent_path()]];
    if (IndexForPath.contains(search_path.parent_path().parent_path())) return Instances[IndexForPath[search_path.parent_path().parent_path()]];
    return nullptr;
}

void Field::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}
