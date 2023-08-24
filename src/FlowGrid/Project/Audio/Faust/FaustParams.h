#pragma once

#include "Core/Primitive/UInt.h"

#include "FaustParam.h"
#include "FaustParamGroup.h"
#include "FaustParamsContainer.h"

#include <stack>

class dsp;
class FaustParamsUI;
struct FaustParamsStyle;
struct NamesAndValues;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).
struct FaustParams : Component, FaustParamsContainer {
    FaustParams(ComponentArgs &&, const FaustParamsStyle &);
    ~FaustParams() override;

    void SetDsp(dsp *);

    Prop(UInt, DspId);

private:
    void Render() const override;

    void Add(FaustParamType type, const char *label, string_view short_label, Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, NamesAndValues names_and_values = {}) override {
        FaustParamGroup &active_group = Groups.empty() ? RootGroup : *Groups.top();
        if (zone == nullptr) { // Group
            AllParams.emplace_back(std::make_unique<FaustParamGroup>(ComponentArgs{&active_group, short_label, label}, Style, type, label));
            Groups.push((FaustParamGroup *)AllParams.back().get());
        } else { // Param
            AllParams.emplace_back(std::make_unique<FaustParam>(ComponentArgs{&active_group, short_label, label}, Style, type, label, zone, min, max, init, step, tooltip, std::move(names_and_values)));
        }
    }

    void PopGroup() override { Groups.pop(); }

    const FaustParamsStyle &Style;
    std::unique_ptr<FaustParamsUI> Impl;

    FaustParamGroup RootGroup{ComponentArgs{this, "Param"}, Style};
    std::stack<FaustParamGroup *> Groups{};
    dsp *Dsp{nullptr};

    std::vector<std::unique_ptr<FaustParamBase>> AllParams{};
};
