#pragma once

#include "FileDialogAction.h"
#include "FileDialogData.h"

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Int.h"
#include "Core/Primitive/String.h"

using ImGuiFileDialogFlags = int;

// `FileDialog` is a window, but it's managed by ImGuiFileDialog, so we don't use a `Window` type.
struct FileDialog : Component, Actionable<Action::FileDialog::Any> {
    using Component::Component;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Set(const FileDialogData &) const;

    struct Demo : Component {
        using Component::Component;

    protected:
        void Render() const override;
    };

    inline static bool Visible;
    inline static bool SaveMode; // The same file dialog instance is used for both saving & opening files.
    inline static U32 MaxNumSelections{1};
    inline static ImGuiFileDialogFlags Flags{FileDialogFlags_Modal};
    inline static std::string Title{"Choose file"};
    inline static std::string Filters;
    inline static std::string FilePath{"."};
    inline static std::string DefaultFileName;

    inline static std::string SelectedFilePath; // Not saved to state, since we never want to replay file selection side effects.

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
