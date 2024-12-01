#pragma once

#include "CoreAction.h"

#include "Action/Actionable.h"

struct TransientStore;

struct CoreActionHandler : Actionable<Action::Core::Any> {
    CoreActionHandler(TransientStore &);
    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    TransientStore &_S;
    const TransientStore &S{_S};
};
