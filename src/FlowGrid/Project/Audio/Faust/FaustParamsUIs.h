#pragma once

#include "FaustListener.h"
#include "FaustParamsUIStyle.h"

struct FaustParamsUIs : Component, FaustDspChangeListener {
    using Component::Component;

    void OnFaustDspChanged(ID, dsp *) override;
    void OnFaustDspAdded(ID, dsp *) override;
    void OnFaustDspRemoved(ID) override;

    Prop(FaustParamsUIStyle, Style);

protected:
    void Render() const override;
};
