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

PrimitiveField::PrimitiveField(ComponentArgs &&args, Primitive value) : ExtendedPrimitiveField(std::move(args)) {
    store::Set(*this, value);
}

Primitive PrimitiveField::Get() const { return store::Get(Path); }

template<IsPrimitive T> void TypedField<T>::Set(const T &value) const {
    store::Set(*this, value);
}

// Explicit instantiations for `Set`.
template void TypedField<bool>::Set(const bool &) const;
template void TypedField<U32>::Set(const U32 &) const;
template void TypedField<std::string>::Set(const std::string &) const;
template void TypedField<float>::Set(const float &) const;
template void TypedField<int>::Set(const int &) const;

void PrimitiveField::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Primitive::Set &a) { store::Set(Path, a.value); },
        [this](const Action::Primitive::Bool::Toggle &) { store::Set(Path, !std::get<bool>(store::Get(Path))); },
    );
}

namespace store {
void Set(const ExtendedPrimitiveField &field, const FieldValue &value) {
    Visit(
        value,
        [&field](const std::pair<float, float> &v) {
            store::Set(field.Path / "X", v.first);
            store::Set(field.Path / "Y", v.second);
        },
        [&field](const auto &v) {
            store::Set(field.Path, v);
        },
    );
}
void Set(const ExtendedPrimitiveField::Entries &values) {
    for (const auto &[field, value] : values) Set(field, value);
}
} // namespace store
