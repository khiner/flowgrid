#pragma once

#include "Container.h"
#include "Core/Action/ActionProducer.h"
#include "Core/Container/PrimitiveVector.h"
#include "Core/Primitive/UInt.h"
#include "Core/ProducerComponentArgs.h"
#include "NavigableAction.h"

template<typename T> struct Navigable
    : Container,
      Actionable<typename Action::Navigable<T>::Any>,
      ActionProducer<typename Action::Navigable<T>::Any> {
    using ActionT = typename Action::Navigable<T>;
    using ArgsT = ProducerComponentArgs<typename ActionT::Any>;
    // See note in `PrimitiveVector` for an explanation of this `using`.
    using typename Actionable<typename ActionT::Any>::ActionType;
    using typename ActionProducer<typename ActionT::Any>::ProducedActionType;

    Navigable(ArgsT &&args) : Container(std::move(args.Args)), ActionProducer<ProducedActionType>(std::move(args.Q)) {}

    void Apply(const ActionType &action) const override {
        Visit(
            action,
            [this](const ActionT::Push &a) { Push(a.value); },
            [this](const ActionT::MoveTo &a) { MoveTo(a.index); },
        );
    }

    bool CanApply(const ActionType &action) const override {
        return Visit(
            action,
            [](const ActionT::Push &) { return true; },
            [this](const ActionT::MoveTo &a) { return CanMoveTo(a.index); },
        );
    }

    template<typename U> void IssuePush(U &&value) const { ActionProducer<ProducedActionType>::Q(typename ActionT::Push{Path, std::forward<U>(value)}); }
    void IssueMoveTo(u32 index) const { ActionProducer<ProducedActionType>::Q(typename ActionT::MoveTo{Path, index}); }
    void IssueStepForward() const { ActionProducer<ProducedActionType>::Q(typename ActionT::MoveTo{Path, u32(Cursor) + 1}); }
    void IssueStepBackward() const { ActionProducer<ProducedActionType>::Q(typename ActionT::MoveTo{Path, u32(Cursor) - 1}); }

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
