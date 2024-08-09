#pragma once

#include "immer/flex_vector.hpp"
#include "immer/vector.hpp"

#include "Core/Action/ActionMenuItem.h"
#include "Core/ActionableComponent.h"
#include "Core/Primitive/String.h"
#include "LineChar.h"
#include "Project/FileDialog/FileDialogData.h"
#include "TextBufferAction.h"
#include "TextInputEdit.h"

struct TextBufferImpl;
struct FileDialog;

using TextBufferLine = immer::flex_vector<char>;
using TextBufferLines = immer::flex_vector<TextBufferLine>;

struct TextBuffer : ActionableComponent<Action::TextBuffer::Any> {
    using Line = TextBufferLine;
    using Lines = TextBufferLines;
    using Cursor = LineCharRange;

    TextBuffer(ArgsT &&, const FileDialog &, const fs::path &);
    ~TextBuffer();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    std::string GetText() const;
    bool Empty() const;

    void Render() const override;
    void RenderMenu() const;
    void RenderDebug() const override;

    std::optional<ActionType> ProduceKeyboardAction() const;

    const FileDialog &FileDialog;
    fs::path _LastOpenedFilePath;
    Prop(String, LastOpenedFilePath, _LastOpenedFilePath);
    Prop_(DebugComponent, Debug, "Editor debug");

    struct Buffer {
        Lines Text{Line{}};
        // If immer vectors provided a diff mechanism like its map does,
        // we could efficiently compute diffs across any two arbitrary text buffers, and we wouldn't need this.
        immer::vector<TextInputEdit> Edits{};
        immer::vector<Cursor> Cursors{{}};
        u32 LastAddedCursorIndex{0};
    };

    Buffer B;

private:
    void Commit() const;

    std::unique_ptr<TextBufferImpl> Impl;

    ActionMenuItem<ActionType>
        ShowOpenDialogMenuItem{*this, Action::TextBuffer::ShowOpenDialog{Id}},
        ShowSaveDialogMenuItem{*this, Action::TextBuffer::ShowSaveDialog{Id}};

    const Menu FileMenu{
        "File",
        {
            ShowOpenDialogMenuItem,
            ShowSaveDialogMenuItem,
        }
    };
};
