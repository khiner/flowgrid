#pragma once

#include "Core/Primitive/UInt.h"

class dsp;
class FaustParamsUIImpl;
struct FaustParamsUIStyle;
struct FaustParam;
struct NamesAndValues;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).
struct FaustParamsUI : Component {
    FaustParamsUI(ComponentArgs &&, const FaustParamsUIStyle &);
    ~FaustParamsUI() override;

    void SetDsp(dsp *);

    Prop(UInt, DspId);

private:
    void Render() const override;

    std::unique_ptr<FaustParamsUIImpl> Impl;
    const FaustParamsUIStyle &Style;
    dsp *Dsp{nullptr};
};
