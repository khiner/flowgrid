#pragma once

#include "FileDialogAction.h"
#include "FileDialogData.h"

#include "Core/Stateful/Field/Bool.h"
#include "Core/Stateful/Field/Int.h"
#include "Core/Stateful/Field/String.h"
#include "Core/Stateful/Window.h"

// `FileDialog` is a window, but it's managed by ImGuiFileDialog, so we don't use a `Window` type.
DefineUI(
    FileDialog,

    void Apply(const Action::FileDialog &) const;
    bool CanApply(const Action::FileDialog &) const;

    void Set(const FileDialogData &) const;

    DefineUI(Demo);

    Prop(Bool, Visible);
    Prop(Bool, SaveMode); // The same file dialog instance is used for both saving & opening files.
    Prop(Int, MaxNumSelections, 1);
    Prop(Int, Flags, FileDialogFlags_Modal);
    Prop(String, Title, "Choose file");
    Prop(String, Filters);
    Prop(String, FilePath, ".");
    Prop(String, DefaultFileName);

    Prop(String, SelectedFilePath);
);

// This demo code is adapted from the [ImGuiFileDialog:main branch](https://github.com/aiekick/ImGuiFileDialog/blob/master/main.cpp)
// It is up-to-date as of https://github.com/aiekick/ImGuiFileDialog/commit/43daff00783dd1c4862d31e69a8186259ab1605b
// Demos related to the C interface have been removed.
namespace IGFD {
void Init();
void Uninit();
} // namespace IGFD

extern const FileDialog &file_dialog;
