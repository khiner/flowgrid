#pragma once

#include "Core/Component.h"
#include "Core/Primitive/String.h"
#include "Project/FileDialog/FileDialogData.h"

struct TextBufferImpl;

struct TextBuffer : Component {
    struct FileConfig {
        FileDialogData OpenConfig, SaveConfig;
    };

    TextBuffer(ComponentArgs &&, const fs::path &);
    ~TextBuffer();

    const std::string &GetLanguageFileExtensionsFilter() const;
    std::string GetText() const;
    bool Empty() const;
    void SetText(const std::string &) const;
    void OpenFile(const fs::path &) const;

    void Render() const override;
    void RenderMenu() const;
    void RenderDebug() const override;

    fs::path _LastOpenedFilePath;
    Prop(String, LastOpenedFilePath, _LastOpenedFilePath);
    Prop_(DebugComponent, Debug, "Editor debug");

private:
    std::unique_ptr<TextBufferImpl> Impl;
};
