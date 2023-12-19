#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Container/ContainerAction.h"
#include "Core/Primitive/PrimitiveAction.h"

struct FieldActionHandler : Actionable<Action::Combine<Action::Primitive::Any, Action::Container::Any>> {
    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };
};
