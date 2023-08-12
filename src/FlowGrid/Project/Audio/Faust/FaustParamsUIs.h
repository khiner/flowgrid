#pragma once

#include "FaustDspChangeListener.h"
#include "FaustParamsUIStyle.h"

class dsp;

struct FaustParamsUIs : Component, FaustDspChangeListener {
    using Component::Component;

    void OnFaustDspChanged(dsp *) override;

    Prop(FaustParamsUIStyle, Style);

protected:
    void Render() const override;
};
