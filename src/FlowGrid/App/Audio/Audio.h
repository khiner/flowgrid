#pragma once

#include "Core/Stateful/Window.h"

#include "AudioAction.h"
#include "AudioDevice.h"
#include "Faust/Faust.h"
#include "Graph/AudioGraph.h"

struct Audio : TabsWindow {
    using TabsWindow::TabsWindow;

    void Apply(const Action::Audio::Any &) const;
    bool CanApply(const Action::Audio::Any &) const;

    void Init() const;
    void Uninit() const;
    void Update() const;
    bool NeedsRestart() const;

    DefineUI(Style);

    Prop(AudioDevice, Device);
    Prop(AudioGraph, Graph);
    Prop(Faust, Faust);
    Prop(Style, Style);

protected:
    void Render() const override;
};

extern const Audio &audio;
