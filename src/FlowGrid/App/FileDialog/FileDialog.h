#pragma once

#include "FileDialogAction.h"
#include "FileDialogData.h"

#include "Core/Field/Bool.h"
#include "Core/Field/Int.h"
#include "Core/Field/String.h"
#include "Core/Window.h"

// `FileDialog` is a window, but it's managed by ImGuiFileDialog, so we don't use a `Window` type.
struct FileDialog : Component, Drawable {
    using Component::Component;

    void Apply(const Action::FileDialog::Any &) const;
    bool CanApply(const Action::FileDialog::Any &) const;

    void Set(const FileDialogData &) const;

    struct Demo : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    Prop(Bool, Visible);
    Prop(Bool, SaveMode); // The same file dialog instance is used for both saving & opening files.
    Prop(Int, MaxNumSelections, 1);
    Prop(Int, Flags, FileDialogFlags_Modal);
    Prop(String, Title, "Choose file");
    Prop(String, Filters);
    Prop(String, FilePath, ".");
    Prop(String, DefaultFileName);

    Prop(String, SelectedFilePath);

protected:
    void Render() const override;
};

// This demo code is adapted from the [ImGuiFileDialog:main branch](https://github.com/aiekick/ImGuiFileDialog/blob/master/main.cpp)
// It is up-to-date as of https://github.com/aiekick/ImGuiFileDialog/commit/43daff00783dd1c4862d31e69a8186259ab1605b
// Demos related to the C interface have been removed.
namespace IGFD {
void Init();
void Uninit();
} // namespace IGFD

extern const FileDialog &file_dialog;
