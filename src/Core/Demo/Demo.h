#pragma once

#include "Core/FileDialog/FileDialog.h"

struct Demo : Component {
    Demo(ComponentArgs &&, const FileDialog &);

    struct ImGuiDemo : Component {
        using Component::Component;

    protected:
        void Render() const override;
    };
    struct ImPlotDemo : Component {
        using Component::Component;

    protected:
        void Render() const override;
    };

    const FileDialog &Dialog;
    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog, Dialog);

protected:
    void Render() const override;
};
