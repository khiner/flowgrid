#include "Field.h"

#include "imgui.h"

#include "Core/Store/Store.h"

using std::vector;

Field::Field(ComponentArgs &&args) : Component(std::move(args)) {
    WithPath[Path] = this;
}
Field::~Field() {
    WithPath.erase(Path);
}

void Field::UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}

PrimitiveField::PrimitiveField(ComponentArgs &&args, Primitive value) : Field(std::move(args)) {
    store::Set(*this, value);
}

Primitive PrimitiveField::Get() const { return store::Get(Path); }

void PrimitiveField::ActionHandler::Apply(const ActionType &action) const {
    Visit(
        action,
        [](const Action::Primitive::SetMany &a) { store::Set(a.values); },
        [](const Action::Primitive::Set &a) { store::Set(a.path, a.value); },
        [](const Action::Primitive::ToggleBool &a) { store::Set(a.path, !std::get<bool>(store::Get(a.path))); },
    );
}

namespace store {
void Set(const PrimitiveField &field, const Primitive &value) { store::Set(field.Path, value); }
void Set(const PrimitiveField::Entries &values) {
    for (const auto &[field, value] : values) store::Set(field.Path, value);
}
} // namespace store
