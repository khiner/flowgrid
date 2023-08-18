#pragma once

#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Primitive/UInt.h"

#include "faust/gui/MetaDataUI.h"
#include "faust/gui/PathBuilder.h"
#include "faust/gui/UI.h"

#include "FaustParam.h"
#include "UI/NamesAndValues.h"

struct FaustParamsUIStyle;

class dsp;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).
// TODO consider breaking out the Faust interface parts into a separate `FaustParamsUIImpl`,
// which the `FaustParamsUI` component holds.
class FAUST_API FaustParamsUI : public Component, public UI, public MetaDataUI, public PathBuilder {
public:
    FaustParamsUI(ComponentArgs &&, const FaustParamsUIStyle &);
    ~FaustParamsUI() override;

    void openHorizontalBox(const char *label) override {
        pushLabel(label);
        ActiveGroup().children.emplace_back(FaustParam::Type_HGroup, label);
        Groups.push(&ActiveGroup().children.back());
    }
    void openVerticalBox(const char *label) override {
        pushLabel(label);
        ActiveGroup().children.emplace_back(FaustParam::Type_VGroup, label);
        Groups.push(&ActiveGroup().children.back());
    }
    void openTabBox(const char *label) override {
        pushLabel(label);
        ActiveGroup().children.emplace_back(FaustParam::Type_TGroup, label);
        Groups.push(&ActiveGroup().children.back());
    }
    void closeBox() override {
        Groups.pop();
        if (popLabel()) {
            computeShortNames();
        }
    }

    // Active widgets
    void addButton(const char *label, Real *zone) override {
        Add(FaustParam::Type_Button, label, zone);
    }
    void addCheckButton(const char *label, Real *zone) override {
        Add(FaustParam::Type_CheckButton, label, zone);
    }
    void addHorizontalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, false);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, true);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        Add(FaustParam::Type_NumEntry, label, zone, min, max, init, step);
    }
    void addSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step, bool is_vertical) {
        if (isKnob(zone)) Add(FaustParam::Type_Knob, label, zone, min, max, init, step);
        else if (isRadio(zone)) addRadioButtons(label, zone, init, min, max, step, fRadioDescription[zone].c_str(), is_vertical);
        else if (isMenu(zone)) addMenu(label, zone, init, min, max, step, fMenuDescription[zone].c_str());
        else Add(is_vertical ? FaustParam::Type_VSlider : FaustParam::Type_HSlider, label, zone, min, max, init, step);
    }
    void addRadioButtons(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text, bool is_vertical) {
        NamesAndValues[zone] = {};
        parseMenuList(text, NamesAndValues[zone].names, NamesAndValues[zone].values);
        Add(is_vertical ? FaustParam::Type_VRadioButtons : FaustParam::Type_HRadioButtons, label, zone, min, max, init, step);
    }
    void addMenu(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text) {
        NamesAndValues[zone] = {};
        parseMenuList(text, NamesAndValues[zone].names, NamesAndValues[zone].values);
        Add(FaustParam::Type_Menu, label, zone, min, max, init, step);
    }

    // Passive widgets
    void addHorizontalBargraph(const char *label, Real *zone, Real min, Real max) override {
        Add(FaustParam::Type_HBargraph, label, zone, min, max);
    }
    void addVerticalBargraph(const char *label, Real *zone, Real min, Real max) override {
        Add(FaustParam::Type_VBargraph, label, zone, min, max);
    }

    // Soundfile
    void addSoundfile(const char *, const char *, Soundfile **) override {}

    // Metadata declaration
    void declare(Real *zone, const char *key, const char *value) override {
        MetaDataUI::declare(zone, key, value);
    }

    void Render() const override;
    void SetDsp(dsp *);

    FaustParam UiParam{FaustParam::Type_None, ""};
    std::unordered_map<const Real *, NamesAndValues> NamesAndValues;

    Prop(UInt, DspId);

    dsp *Dsp{nullptr};

private:
    void Add(const FaustParam::Type type, const char *label, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        ActiveGroup().children.emplace_back(type, label, zone, min, max, init, step, fTooltip.contains(zone) ? fTooltip.at(zone).c_str() : nullptr);
        const std::string path = buildPath(label);
        fFullPaths.push_back(path);
    }

    // Param UI calculations.
    float CalcWidth(const FaustParam &, const bool include_label) const;
    float CalcHeight(const FaustParam &) const;
    float CalcLabelHeight(const FaustParam &) const;

    // Param drawing.
    void DrawUiItem(const FaustParam &, const char *label, const float suggested_height) const;

    FaustParam &ActiveGroup() { return Groups.empty() ? UiParam : *Groups.top(); }

    std::stack<FaustParam *> Groups{};
    const FaustParamsUIStyle &Style;
};
