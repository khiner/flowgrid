#pragma once

#include "Core/Window.h"
#include "FileDialog/FileDialog.h"

struct Demo : TabsWindow {
    Demo(ComponentArgs &&);

    struct ImGuiDemo : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };
    struct ImPlotDemo : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};
