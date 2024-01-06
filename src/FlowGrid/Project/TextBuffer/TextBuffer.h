#pragma once

#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/ActionableProducer.h"
#include "Core/Primitive/Primitive.h"
#include "Core/ProducerComponentArgs.h"
#include "Project/FileDialog/FileDialogData.h"
#include "TextBufferAction.h"

struct FileDialog;
struct TextEditor;

struct TextBuffer : Primitive<string>, ActionableProducer<Action::TextBuffer::Any> {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    struct FileConfig {
        FileDialogData OpenConfig, SaveConfig;
    };

    TextBuffer(ArgsT &&, const FileDialog &, FileConfig &&, string_view value = "");
    TextBuffer(ArgsT &&, const FileDialog &, string_view value = "");
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

    const FileDialog &FileDialog;
    FileConfig FileConf;
    std::unique_ptr<TextEditor> Editor;
};
