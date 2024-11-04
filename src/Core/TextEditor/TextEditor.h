#pragma once

#include "TextBuffer.h"

// Will hold multiple text buffers.
struct TextEditor : Component {
    TextEditor(ComponentArgs &&, const fs::path &);
    ~TextEditor();

    void RenderDebug() const override;

    bool Empty() const;
    std::string GetText() const;

    fs::path _LastOpenedFilePath;
    Prop(TextBuffer, Buffer, _LastOpenedFilePath);

private:
    void Render() const override;
    void RenderMenu() const;
};
