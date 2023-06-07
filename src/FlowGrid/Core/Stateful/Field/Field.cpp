#include "Field.h"

#include "imgui.h"

#include "Core/Store/Store.h"

using std::vector;

namespace Stateful::Field {
void UpdateGesturing() {
    if (ImGui::IsItemActivated()) IsGesturing = true;
    if (ImGui::IsItemDeactivated()) IsGesturing = false;
}

Base::Base(Stateful::Base *parent, string_view path_segment, string_view name_help)
    : Stateful::Base(parent, path_segment, name_help) {
    WithPath[Path] = this;
}
Base::~Base() {
    WithPath.erase(Path);
}

PrimitiveBase::PrimitiveBase(Stateful::Base *parent, string_view id, string_view name_help, Primitive value)
    : Base(parent, id, name_help) {
    store::Set(*this, value);
}

Primitive PrimitiveBase::Get() const { return store::Get(Path); }

void PrimitiveBase::Apply(const Action::Value &action) {
    Match(
        action,
        [](const Action::SetValue &a) { store::Set(a.path, a.value); },
        [](const Action::ToggleValue &a) { store::Set(a.path, !std::get<bool>(store::Get(a.path))); },
    );
}
bool PrimitiveBase::CanApply(const Action::Value &) { return true; }
} // namespace Stateful::Field

namespace store {
void Set(const Stateful::Field::Base &field, const Primitive &value) { store::Set(field.Path, value); }
void Set(const Stateful::Field::Entries &values) {
    for (const auto &[field, value] : values) store::Set(field.Path, value);
}
} // namespace store
