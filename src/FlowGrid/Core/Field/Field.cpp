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

PrimitiveField::PrimitiveField(ComponentArgs &&args, Primitive value) : Field(std::move(args)) {
    store::Set(*this, value);
}

Primitive PrimitiveField::Get() const { return store::Get(Path); }

template<IsPrimitive T> void TypedField<T>::Set(const T &value) {
    store::Set(*this, value);
    Value = value;
}

// Explicit instantiations for `Set`.
template void TypedField<bool>::Set(const bool &);
template void TypedField<U32>::Set(const U32 &);
template void TypedField<std::string>::Set(const std::string &);
template void TypedField<float>::Set(const float &);
template void TypedField<int>::Set(const int &);

void PrimitiveField::ActionHandler::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Primitive::Set &a) { store::Set(a.path, a.value); },
        [](const Action::Primitive::Bool::Toggle &a) { store::Set(a.path, !std::get<bool>(store::Get(a.path))); },
    );
}

namespace store {
void Set(const PrimitiveField &field, const Primitive &value) { store::Set(field.Path, value); }
void Set(const PrimitiveField::Entries &values) {
    for (const auto &[field, value] : values) store::Set(field.Path, value);
}
} // namespace store
