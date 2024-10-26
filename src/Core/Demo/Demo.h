#pragma once

#include "Core/FileDialog/FileDialogDemo.h"

struct Demo : ActionProducerComponent<Action::FileDialog::Any> {
    Demo(ArgsT &&);

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
    ProducerProp(FileDialogDemo, FileDialog);

protected:
    void Render() const override;
};
