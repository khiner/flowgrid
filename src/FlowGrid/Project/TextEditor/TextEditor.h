#pragma once

#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/ActionableProducer.h"
#include "Core/ActionableComponent.h"
#include "Core/Primitive/String.h"
#include "Core/ProducerComponentArgs.h"
#include "Project/FileDialog/FileDialogData.h"
#include "TextBuffer.h"
#include "TextEditorAction.h"

struct FileDialog;

struct TextEditor : ActionableComponent<Action::TextEditor::Any> {
    struct FileConfig {
        FileDialogData OpenConfig, SaveConfig;
    };

    TextEditor(ArgsT &&, const FileDialog &, const fs::path &file_path);
    TextEditor(ArgsT &&, const FileDialog &, FileConfig &&, const fs::path &file_path);
    ~TextEditor();

    FileConfig CreateDefaultFileConfig(const fs::path &) const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    void RenderDebug() const override;

    bool Empty() const;
    std::string GetText() const;

    fs::path _LastOpenedFilePath;
    Prop(TextBuffer, Buffer, _LastOpenedFilePath);

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
};
