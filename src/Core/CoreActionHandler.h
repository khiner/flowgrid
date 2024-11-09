#pragma once

#include "CoreAction.h"

#include "Action/Actionable.h"

struct Store;

struct CoreActionHandler : Actionable<Action::Core::Any> {
    CoreActionHandler(Store &);
    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    Store &_S;
    const Store &S{_S};
};
