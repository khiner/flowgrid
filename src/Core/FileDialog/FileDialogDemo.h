#pragma once

#include "Core/ActionProducerComponent.h"

#include "FileDialogAction.h"

struct FileDialog;
struct FileDialogData;

struct FileDialogDemo : ActionProducerComponent<Action::FileDialog::Any> {
    using ActionProducerComponent::ActionProducerComponent;

protected:
    void Render() const override;

private:
    void OpenDialog(const FileDialogData &) const;
};
