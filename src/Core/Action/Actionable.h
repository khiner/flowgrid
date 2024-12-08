#pragma once

struct TransientStore;

template<typename T> struct Actionable {
    using ActionType = T;

    virtual void Apply(TransientStore &, const ActionType &) const = 0;
    virtual bool CanApply(const ActionType &) const = 0;
};
