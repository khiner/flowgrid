#pragma once

#include "Core/ActionProducerComponent.h"
#include "FileDialogAction.h"
#include "FileDialogData.h"

using ImGuiFileDialogFlags = int;

struct FileDialog : ActionProducerComponent<Action::FileDialog::Any> {
    using ActionProducerComponent::ActionProducerComponent;

    void Set(FileDialogData &&) const;
    void SetJson(json &&) const override;

    struct Demo : Component {
        Demo(ComponentArgs &&, const FileDialog &);

        const FileDialog &FileDialog;

    protected:
        void Render() const override;

    private:
        void OpenDialog(const FileDialogData &) const;
    };

    inline static bool Visible;
    inline static FileDialogData Data;

    inline static std::string SelectedFilePath; // Not saved to state, since we never want to replay file selection side effects.

protected:
    void Render() const override;
};
