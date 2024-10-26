#pragma once

#include "Core/ActionProducerComponent.h"
#include "TextBuffer.h"

// Will hold multiple text buffers.
struct TextEditor : ActionProducerComponent<Action::TextBuffer::Any> {
    TextEditor(ArgsT &&, const fs::path &);
    ~TextEditor();

    void RenderDebug() const override;

    bool Empty() const;
    std::string GetText() const;

    fs::path _LastOpenedFilePath;
    ProducerProp(TextBuffer, Buffer, _LastOpenedFilePath);

private:
    void Render() const override;
    void RenderMenu() const;
};
