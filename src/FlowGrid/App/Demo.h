#pragma once

#include "Core/Window.h"
#include "FileDialog/FileDialog.h"

struct Demo : TabsWindow {
    Demo(ComponentArgs &&);

    struct ImGuiDemo : UIComponent {
        using UIComponent::UIComponent;

    protected:
        void Render() const override;
    };
    struct ImPlotDemo : UIComponent {
        using UIComponent::UIComponent;

    protected:
        void Render() const override;
    };

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};
