#pragma once

struct ChangeListener {
    // Called when at least one of the listened components has changed.
    // Changed component(s) are not passed to the callback, but it's called while the components are still marked as changed,
    // so listeners can use `component.IsChanged()` to check which listened components were changed if they wish.
    virtual void OnComponentChanged() = 0;
};
