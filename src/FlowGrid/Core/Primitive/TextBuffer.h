#pragma once

#include "Core/Action/Actionable.h"
#include "Primitive.h"
#include "TextBufferAction.h"

struct TextEditor;

struct TextBuffer : Primitive<string>, Actionable<Action::TextBuffer::Any> {
    TextBuffer(ComponentArgs &&, const Menu &file_menu, string_view value = "");
    ~TextBuffer();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void RenderDebug() const override;

    operator bool() const { return !Value.empty(); }
    operator string_view() const { return Value; }

    Prop_(DebugComponent, Debug, "Editor debug");

private:
    void Render() const override;
    void RenderMenu() const;

    const Menu &FileMenu;
    std::unique_ptr<TextEditor> Editor;
};
