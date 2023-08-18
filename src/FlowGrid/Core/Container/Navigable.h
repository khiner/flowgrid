#pragma once

#include "Core/Container/PrimitiveVector.h"
#include "Core/Primitive/UInt.h"
#include "NavigableAction.h"

template<typename T> struct Navigable : Field, Actionable<typename Action::Navigable<T>::Any> {
    using Field::Field;

    // See note in `PrimitiveVector` for an explanation of this `using`.
    using typename Actionable<typename Action::Navigable<T>::Any>::ActionType;

    void Apply(const ActionType &action) const override {
        Visit(
            action,
            [this](const Action::Navigable<T>::Push &a) { Push(a.value); },
            [this](const Action::Navigable<T>::MoveTo &a) { MoveTo(a.index); },
        );
    }

    bool CanApply(const ActionType &action) const override {
        return Visit(
            action,
            [](const Action::Navigable<T>::Push &) { return true; },
            [this](const Action::Navigable<T>::MoveTo &a) { return CanMoveTo(a.index); },
        );
    }

    template<typename U> void IssuePush(U &&value) const { typename Action::Navigable<T>::Push{Path, std::forward<U>(value)}.q(); }
    void IssueMoveTo(u32 index) const { typename Action::Navigable<T>::MoveTo{Path, index}.q(); }
    void IssueStepForward() const { typename Action::Navigable<T>::MoveTo{Path, u32(Cursor) + 1}.q(); }
    void IssueStepBackward() const { typename Action::Navigable<T>::MoveTo{Path, u32(Cursor) - 1}.q(); }

    inline auto begin() { return Value.begin(); }
    inline auto end() { return Value.end(); }

    inline u32 GetCursor() const { return Cursor; }

    inline bool Empty() const { return Value.Empty(); }
    inline bool CanMoveTo(u32 index) const { return index < Value.Size(); }
    inline bool CanStepBackward() const { return u32(Cursor) > 0; }
    inline bool CanStepForward() const { return !Value.Empty() && u32(Cursor) < Value.Size() - 1u; }

    inline auto Back() const { return Value.back(); }

    inline void Clear_() {
        Value.Clear_();
        Cursor.Set_(0);
    }

    template<typename U> void Push(U &&value) const {
        if (Value.Empty()) {
            Value.PushBack(std::forward<U>(value));
            Cursor.Set(0);
        } else {
            Value.Resize(u32(Cursor) + 1);
            // `PushBack` won't work here, since the `PrimitiveVector` won't have the correct size cached after non-mutating `Resize`.
            Value.Set(u32(Cursor) + 1, std::forward<U>(value));
            Cursor.Set(u32(Cursor) + 1); // Avoid clamping, since `Value.Size()` can't be trusted here.
        }
    }
    template<typename U> void Push_(U &&value) {
        Value.Resize_(Cursor);
        Value.PushBack_(std::forward<U>(value));
        Cursor.Set_(Value.Size() - 1);
    }
    inline void Pop() const {
        if (Value.Empty()) return;
        if (u32(Cursor) > 0 && Cursor == Value.Size() - 1) Cursor.Set(u32(Cursor) - 1);
        Value.PopBack();
    }

    inline void StepForward() const { Move(1); }
    inline void StepBackward() const { Move(-1); }

    inline void MoveTo(u32 index) const { Cursor.Set(std::clamp(int(index), 0, int(Value.Size()) - 1)); }
    inline void Move(int offset) const { Cursor.Set(std::clamp(int(Cursor) + offset, 0, int(Value.Size()) - 1)); }

    inline auto operator[](u32 index) { return Value[index]; }
    T operator*() const {
        return Value[Cursor];
    }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        Component::RenderValueTree(annotate, auto_select);
    }

private:
    Prop(PrimitiveVector<T>, Value);
    Prop(UInt, Cursor);
};
