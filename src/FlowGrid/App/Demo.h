#pragma once

#include "FileDialog/FileDialog.h"

struct Demo : Component {
    Demo(ComponentArgs &&);

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

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);

protected:
    void Render() const override;
};
