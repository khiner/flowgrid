#pragma once

#include "Core/Action/ActionableProducer.h"
#include "Core/Primitive/String.h"
#include "Core/ProducerComponentArgs.h"
#include "TextBuffer.h"

struct TextEditor : ActionableComponent<Action::TextBuffer::Any> {
    TextEditor(ArgsT &&, const FileDialog &, const fs::path &);
    ~TextEditor();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void RenderDebug() const override;

    bool Empty() const;
    std::string GetText() const;

    const FileDialog &FileDialog;
    fs::path _LastOpenedFilePath;
    ProducerProp(TextBuffer, Buffer, FileDialog, _LastOpenedFilePath);

private:
    void Render() const override;
    void RenderMenu() const;
};
