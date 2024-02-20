#pragma once

#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/ActionableProducer.h"
#include "Core/ActionableComponent.h"
#include "Core/Primitive/String.h"
#include "Core/ProducerComponentArgs.h"
#include "Project/FileDialog/FileDialogData.h"
#include "Project/TextEditor/LanguageID.h"
#include "TextEditorAction.h"

struct FileDialog;
struct TextBuffer;

struct TextEditor : ActionableComponent<Action::TextEditor::Any> {
    struct FileConfig {
        FileDialogData OpenConfig, SaveConfig;
    };

    TextEditor(ArgsT &&, const FileDialog &, FileConfig &&, string_view text = "", LanguageID language_id = LanguageID::None);
    TextEditor(ArgsT &&, const FileDialog &, string_view text = "", LanguageID language_id = LanguageID::None);
    TextEditor(ArgsT &&, const FileDialog &, const fs::path &file_path);
    ~TextEditor();

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
        ShowOpenDialogMenuItem{*this, CreateProducer<ActionType>(), Action::TextEditor::ShowOpenDialog{Path}},
        ShowSaveDialogMenuItem{*this, CreateProducer<ActionType>(), Action::TextEditor::ShowSaveDialog{Path}};

    const Menu FileMenu{
        "File",
        {
            ShowOpenDialogMenuItem,
            ShowSaveDialogMenuItem,
        }
    };

    const FileDialog &FileDialog;
    FileConfig FileConf;
    std::unique_ptr<TextBuffer> Buffer;
};
