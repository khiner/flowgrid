#pragma once

#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/ActionableProducer.h"
#include "Core/ActionableComponent.h"
#include "Core/Primitive/String.h"
#include "Core/ProducerComponentArgs.h"
#include "Project/FileDialog/FileDialogData.h"
#include "Project/TextEditor/LanguageID.h"
#include "TextBufferAction.h"

struct FileDialog;
struct TextEditor;

struct TextBuffer : ActionableComponent<Action::TextBuffer::Any> {
    struct FileConfig {
        FileDialogData OpenConfig, SaveConfig;
    };

    TextBuffer(ArgsT &&, const FileDialog &, FileConfig &&, string_view text = "", LanguageID language_id = LanguageID::None);
    TextBuffer(ArgsT &&, const FileDialog &, string_view text = "", LanguageID language_id = LanguageID::None);
    TextBuffer(ArgsT &&, const FileDialog &, const fs::path &file_path);
    ~TextBuffer();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void RenderDebug() const override;

    operator bool() const { return bool(Text); }
    operator string_view() const { return Text; }
    operator string() const { return Text; }

    Prop_(DebugComponent, Debug, "Editor debug");
    Prop(String, Text);
    Prop(String, LastOpenedFilePath);

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
