#pragma once

#include "Core/Action/ActionMenuItem.h"
#include "Core/ActionableComponent.h"
#include "Core/Primitive/String.h"
#include "Project/FileDialog/FileDialogData.h"
#include "TextBufferAction.h"
#include "TextBufferData.h"

struct TextBufferImpl;
struct FileDialog;

struct TextBuffer : ActionableComponent<Action::TextBuffer::Any> {
    using Line = TextBufferLine;
    using Lines = TextBufferLines;
    using Cursor = LineCharRange;

    TextBuffer(ArgsT &&, const FileDialog &, const fs::path &);
    ~TextBuffer();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    TextBufferData GetBuffer() const;
    std::string GetText() const;
    bool Exists() const;
    bool Empty() const;

    void Refresh() override;
    void Render() const override;
    void RenderMenu() const;
    void RenderDebug() const override;

    std::optional<ActionType> ProduceKeyboardAction() const;

    const FileDialog &FileDialog;
    fs::path _LastOpenedFilePath;
    Prop(String, LastOpenedFilePath, _LastOpenedFilePath);
    Prop_(DebugComponent, Debug, "Editor debug");

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
