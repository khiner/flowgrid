#pragma once

#include "Core/Container/Vector.h"
#include "Core/Primitive/UInt.h"

template<typename T> struct Navigable : Component {
    Navigable(ComponentArgs &&args) : Component(std::move(args)) {
        Refresh();
    }
    ~Navigable() {
        Erase();
    }

    void IssueClear() const;
    void IssuePush(T) const;
    void IssueMoveTo(u32 index) const;
    void IssueStepForward() const;
    void IssueStepBackward() const;

    bool Empty() const { return Value.Empty(); }
    bool CanStepBackward() const { return u32(Cursor) > 0; }
    bool CanStepForward() const { return !Value.Empty() && u32(Cursor) < Value.Size() - 1u; }

    auto operator[](u32 index) { return Value[index]; }
    T operator*() const { return Value[Cursor]; }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        Component::RenderValueTree(annotate, auto_select); // todo
    }

    Prop(Vector<T>, Value);
    Prop(UInt, Cursor);
};
