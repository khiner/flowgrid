#include "Field.h"

#include "imgui.h"

#include "Core/Store/Store.h"

using std::vector;

Field::Field(Component *parent, string_view path_leaf, string_view meta_str)
    : Component(parent, path_leaf, meta_str) {
    WithPath[Path] = this;
}
Field::~Field() {
    WithPath.erase(Path);
}

void Field::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}

PrimitiveField::PrimitiveField(Component *parent, string_view id, string_view meta_str, Primitive value)
    : Field(parent, id, meta_str) {
    store::Set(*this, value);
}

Primitive PrimitiveField::Get() const { return store::Get(Path); }

void PrimitiveField::Apply(const Action::Primitive::Any &action) {
    Visit(
        action,
        [](const Action::Primitive::SetMany &a) { store::Set(a.values); },
        [](const Action::Primitive::Set &a) { store::Set(a.path, a.value); },
        [](const Action::Primitive::ToggleBool &a) { store::Set(a.path, !std::get<bool>(store::Get(a.path))); },
    );
}
bool PrimitiveField::CanApply(const Action::Primitive::Any &) { return true; }

namespace store {
void Set(const PrimitiveField &field, const Primitive &value) { store::Set(field.Path, value); }
void Set(const PrimitiveField::Entries &values) {
    for (const auto &[field, value] : values) store::Set(field.Path, value);
}
} // namespace store
