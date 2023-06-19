#include "Field.h"

#include "imgui.h"

#include "Core/Store/Store.h"

using std::vector;

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    Index = Instances.size();
    Instances.push_back(this);
    IndexForPath[Path] = Index;
}
Field::~Field() {
    IndexForPath.erase(Path);
    Instances.erase(Instances.begin() + Index);
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

template<IsPrimitive T> T TypedField<T>::Get() const { return std::get<T>(store::Get(Path)); }

// Explicit instantiations for `Get`.
template bool TypedField<bool>::Get() const;
template int TypedField<int>::Get() const;
template U32 TypedField<U32>::Get() const;
template float TypedField<float>::Get() const;
template std::string TypedField<std::string>::Get() const;

template<IsPrimitive T> void TypedField<T>::Set(const T &value) const { store::Set(Path, value); }

// Explicit instantiations for `Set`.
template void TypedField<bool>::Set(const bool &) const;
template void TypedField<int>::Set(const int &) const;
template void TypedField<U32>::Set(const U32 &) const;
template void TypedField<float>::Set(const float &) const;
template void TypedField<std::string>::Set(const std::string &) const;
