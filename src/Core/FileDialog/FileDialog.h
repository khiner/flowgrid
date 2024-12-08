#pragma once

#include "Core/Action/ActionProducer.h"
#include "Core/Component.h"

#include "FileDialogAction.h"
#include "FileDialogData.h"

using ImGuiFileDialogFlags = int;

struct FileDialog : ActionProducer<Action::FileDialog::Any> {
    using ActionProducer::ActionProducer;

    void Set(FileDialogData &&) const;
    void SetJson(TransientStore &, json &&) const;

    inline static bool Visible;
    inline static FileDialogData Data;
    inline static std::string SelectedFilePath;

    void Render() const;
};
