#pragma once

#include "Core/Action/Actionable.h"
#include "FieldAction.h"


struct FieldActionHandler : Actionable<Action::Field::Any> {
    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };
};
