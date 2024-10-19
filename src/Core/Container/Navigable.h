#pragma once

#include "Core/Action/ActionProducer.h"
#include "Core/Container/Vector.h"
#include "Core/Primitive/UInt.h"
#include "Core/ProducerComponentArgs.h"
#include "NavigableAction.h"

// Not using `ActionProducerComponent`, since even worse of a templating mess than it already is here.
template<typename T> struct Navigable : Component, ActionProducer<typename Action::Navigable<T>::Any> {
    using ActionT = typename Action::Navigable<T>;
    using ArgsT = ProducerComponentArgs<typename ActionT::Any>;
    using typename ActionProducer<typename ActionT::Any>::ProducedActionType;

    Navigable(ArgsT &&args) : Component(std::move(args.Args)), ActionProducer<ProducedActionType>(std::move(args.Q)) {
        Refresh();
    }
    ~Navigable() {
        Erase();
    }

    void IssueClear() const { ActionProducer<ProducedActionType>::Q(typename ActionT::Clear{Id}); }
    template<typename U> void IssuePush(U &&value) const { ActionProducer<ProducedActionType>::Q(typename ActionT::Push{Id, std::forward<U>(value)}); }
    void IssueMoveTo(u32 index) const { ActionProducer<ProducedActionType>::Q(typename ActionT::MoveTo{Id, index}); }
    void IssueStepForward() const { ActionProducer<ProducedActionType>::Q(typename ActionT::MoveTo{Id, u32(Cursor) + 1}); }
    void IssueStepBackward() const { ActionProducer<ProducedActionType>::Q(typename ActionT::MoveTo{Id, u32(Cursor) - 1}); }

    auto begin() { return Value.begin(); }
    auto end() { return Value.end(); }

    bool Empty() const { return Value.Empty(); }
    bool CanStepBackward() const { return u32(Cursor) > 0; }
    bool CanStepForward() const { return !Value.Empty() && u32(Cursor) < Value.Size() - 1u; }

    auto Back() const { return Value.back(); }

    auto operator[](u32 index) { return Value[index]; }
    T operator*() const { return Value[Cursor]; }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        Component::RenderValueTree(annotate, auto_select);
    }

    Prop(Vector<T>, Value);
    Prop(UInt, Cursor);
};
