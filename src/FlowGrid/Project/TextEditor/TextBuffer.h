#pragma once

#include "Core/Action/ActionMenuItem.h"
#include "Core/ActionableComponent.h"
#include "Core/Primitive/String.h"
#include "Project/FileDialog/FileDialogData.h"
#include "TextBufferAction.h"

struct TextBufferImpl;
struct FileDialog;

struct TextBuffer : ActionableComponent<Action::TextBuffer::Any> {
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

private:
    std::unique_ptr<TextBufferImpl> Impl;

    ActionMenuItem<ActionType>
        ShowOpenDialogMenuItem{*this, CreateProducer<ActionType>(), Action::TextBuffer::ShowOpenDialog{Path}},
        ShowSaveDialogMenuItem{*this, CreateProducer<ActionType>(), Action::TextBuffer::ShowSaveDialog{Path}};

    const Menu FileMenu{
        "File",
        {
            ShowOpenDialogMenuItem,
            ShowSaveDialogMenuItem,
        }
    };
};
