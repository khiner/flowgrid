#pragma once

template<typename T> struct Actionable {
    using ActionType = T;
    virtual void Apply(const ActionType &) const = 0;
    virtual bool CanApply(const ActionType &) const = 0;
};
