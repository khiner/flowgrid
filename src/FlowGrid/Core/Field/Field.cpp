#include "Field.h"

#include "imgui.h"

#include "Core/Store/Store.h"

using std::vector;

Field::Field(Stateful::Base *parent, string_view path_segment, string_view name_help)
    : Stateful::Base(parent, path_segment, name_help) {
    WithPath[Path] = this;
}
Field::~Field() {
    WithPath.erase(Path);
}

void Field::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}

PrimitiveField::PrimitiveField(Stateful::Base *parent, string_view id, string_view name_help, Primitive value)
    : Field(parent, id, name_help) {
    store::Set(*this, value);
}

Primitive PrimitiveField::Get() const { return store::Get(Path); }

void PrimitiveField::Apply(const Action::Value &action) {
    Match(
        action,
        [](const Action::SetValue &a) { store::Set(a.path, a.value); },
        [](const Action::ToggleValue &a) { store::Set(a.path, !std::get<bool>(store::Get(a.path))); },
    );
}
bool PrimitiveField::CanApply(const Action::Value &) { return true; }

namespace store {
void Set(const PrimitiveField &field, const Primitive &value) { store::Set(field.Path, value); }
void Set(const PrimitiveField::Entries &values) {
    for (const auto &[field, value] : values) store::Set(field.Path, value);
}
} // namespace store
