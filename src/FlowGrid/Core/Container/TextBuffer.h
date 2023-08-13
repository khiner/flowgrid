#pragma once

#include "Core/Primitive/PrimitiveField.h"
#include "TextBufferAction.h"

struct TextBuffer : PrimitiveField<string>, Actionable<Action::TextBuffer::Any> {
    TextBuffer(ComponentArgs &&, string_view value = "");

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void RenderDebug() const override;

    operator bool() const { return !Value.empty(); }
    operator string_view() const { return Value; }

    Prop_(DebugComponent, Debug, "Editor debug");

private:
    void Render() const override;
    void RenderMenu() const;
};
