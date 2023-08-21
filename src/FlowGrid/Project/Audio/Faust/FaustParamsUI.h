#pragma once

#include "Core/Primitive/UInt.h"

#include "FaustParam.h"
#include "FaustParamsContainer.h"

#include <stack>

class dsp;
class FaustParamsUIImpl;
struct FaustParamsUIStyle;
struct FaustParam;
struct NamesAndValues;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).
struct FaustParamsUI : Component, FaustParamsContainer {
    FaustParamsUI(ComponentArgs &&, const FaustParamsUIStyle &);
    ~FaustParamsUI() override;

    void SetDsp(dsp *);

    Prop(UInt, DspId);

private:
    void Render() const override;

    void Add(FaustParamType type, const char *label, Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {}) override {
        FaustParam &active_group = Groups.empty() ? RootParam : *Groups.top();
        if (zone == nullptr) { // Group
            active_group.Children.emplace_back(Style, type, label);
            Groups.push(&active_group.Children.back());
        } else { // Param
            active_group.Children.emplace_back(Style, type, label, zone, min, max, init, step, tooltip, std::move(names_and_values));
        }
    }

    void PopGroup() override { Groups.pop(); }

    const FaustParamsUIStyle &Style;
    std::unique_ptr<FaustParamsUIImpl> Impl;
    FaustParam RootParam{Style};
    std::stack<FaustParam *> Groups{};
    dsp *Dsp{nullptr};
};
