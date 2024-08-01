#pragma once

#include "Core/Action/ActionProducer.h"
#include "Core/Container/Vec2Action.h"
#include "Helper/Path.h"
#include "PrimitiveAction.h"

struct PrimitiveActionQueuer {
    using ProducedActionType = Action::Combine<Action::Primitive::Any, Action::Vec2::Any>;
    using EnqueueFn = ActionProducer<ProducedActionType>::EnqueueFn;

    PrimitiveActionQueuer(EnqueueFn q) : Q(q) {}

    bool QueueToggle(ID id) { return Q(Action::Primitive::Bool::Toggle{id}); }
    bool QueueSet(ID id, u32 value) { return Q(Action::Primitive::UInt::Set{id, value}); }
    bool QueueSet(ID id, s32 value) { return Q(Action::Primitive::Int::Set{id, value}); }
    bool QueueSet(ID id, float value) { return Q(Action::Primitive::Float::Set{id, value}); }
    bool QueueSet(ID id, const std::string &value) { return Q(Action::Primitive::String::Set{id, value}); }

    EnqueueFn Q;
};
