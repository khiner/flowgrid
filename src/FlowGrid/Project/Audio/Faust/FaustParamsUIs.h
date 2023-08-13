#pragma once

#include "FaustListener.h"
#include "FaustParamsUIStyle.h"

class dsp;

struct FaustParamsUIs : Component, FaustDspChangeListener {
    using Component::Component;

    void OnFaustDspChanged(ID, dsp *) override;

    Prop(FaustParamsUIStyle, Style);

protected:
    void Render() const override;
};
