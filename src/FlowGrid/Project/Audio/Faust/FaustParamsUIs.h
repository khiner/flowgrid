#pragma once

#include "Core/Container/Vector.h"

#include "FaustListener.h"
#include "FaustParamsUIStyle.h"
#include "FaustParamsUI.h"

class dsp;

struct FaustParamsUIs : Component, FaustDspChangeListener {
    using Component::Component;

    static std::unique_ptr<FaustParamsUI> CreateChild(Component *, string_view path_prefix_segment, string_view path_segment);

    void OnFaustDspChanged(ID, dsp *) override;
    void OnFaustDspAdded(ID, dsp *) override;
    void OnFaustDspRemoved(ID) override;

    FaustParamsUI *FindUi(ID dsp_id) const;

    Prop(FaustParamsUIStyle, Style);
    Prop(Vector<FaustParamsUI>, Uis, CreateChild);

protected:
    void Render() const override;
};
