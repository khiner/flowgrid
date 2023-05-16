#pragma once

#include "FileDialogData.h"

#include "../WindowMember.h"

// `FileDialog` is a window, but it's managed by ImGuiFileDialog, so we don't use a `Window` type.
UIMember(
    FileDialog,

    void Update(const FileDialogAction &, TransientStore &) const;
    void Set(const FileDialogData &, TransientStore &) const;

    UIMember(Demo);

    Prop(Bool, Visible);
    Prop(Bool, SaveMode); // The same file dialog instance is used for both saving & opening files.
    Prop(Int, MaxNumSelections, 1);
    Prop(Int, Flags, FileDialogFlags_Default);
    Prop(String, Title, "Choose file");
    Prop(String, Filters);
    Prop(String, FilePath, ".");
    Prop(String, DefaultFileName);
);

// This demo code is adapted from the [ImGuiFileDialog:main branch](https://github.com/aiekick/ImGuiFileDialog/blob/master/main.cpp)
// It is up-to-date as of https://github.com/aiekick/ImGuiFileDialog/commit/43daff00783dd1c4862d31e69a8186259ab1605b
// Demos related to the C interface have been removed.
namespace IGFD {
void InitializeDemo();
void CleanupDemo();
} // namespace IGFD
