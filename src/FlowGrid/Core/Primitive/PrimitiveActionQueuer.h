#pragma once

#include "Core/Action/ActionProducer.h"
#include "Helper/Path.h"
#include "PrimitiveAction.h"

struct PrimitiveActionQueuer {
    PrimitiveActionQueuer(ActionProducer<Action::Primitive::Any>::EnqueueFn enqueue) : Enqueue(enqueue) {}

    bool QueueToggle(const StorePath &path) { return Enqueue(Action::Primitive::Bool::Toggle{path}); }
    bool QueueSet(const StorePath &path, u32 value) { return Enqueue(Action::Primitive::UInt::Set{path, value}); }
    bool QueueSet(const StorePath &path, s32 value) { return Enqueue(Action::Primitive::Int::Set{path, value}); }
    bool QueueSet(const StorePath &path, float value) { return Enqueue(Action::Primitive::Float::Set{path, value}); }
    bool QueueSet(const StorePath &path, const std::string &value) { return Enqueue(Action::Primitive::String::Set{path, value}); }

    ActionProducer<Action::Primitive::Any>::EnqueueFn Enqueue;
};
