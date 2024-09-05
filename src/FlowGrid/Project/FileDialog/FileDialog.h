#pragma once

#include "Core/ActionProducerComponent.h"
#include "FileDialogAction.h"
#include "FileDialogData.h"

using ImGuiFileDialogFlags = int;

struct FileDialog : ActionProducerComponent<Action::FileDialog::Any> {
    using ActionProducerComponent::ActionProducerComponent;

    void Set(const FileDialogData &) const;

    struct Demo : Component {
        Demo(ComponentArgs &&, const FileDialog &);

        const FileDialog &FileDialog;

    protected:
        void Render() const override;

    private:
        void OpenDialog(const FileDialogData &) const;
    };

    inline static ID OwnerId;
    inline static bool Visible;
    inline static bool SaveMode; // The same file dialog instance is used for both saving & opening files.
    inline static u32 MaxNumSelections{1};
    inline static ImGuiFileDialogFlags Flags{FileDialogFlags_Modal};
    inline static std::string Title{"Choose file"};
    inline static std::string Filters;
    inline static std::string FilePath{"."};
    inline static std::string DefaultFileName;

    inline static std::string SelectedFilePath; // Not saved to state, since we never want to replay file selection side effects.

protected:
    void Render() const override;
};
