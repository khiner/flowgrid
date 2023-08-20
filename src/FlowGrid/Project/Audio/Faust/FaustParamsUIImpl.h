#pragma once

#include "FaustParam.h"
#include "UI/NamesAndValues.h"

#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "faust/gui/MetaDataUI.h"
#include "faust/gui/PathBuilder.h"
#include "faust/gui/UI.h"

class FAUST_API FaustParamsUIImpl : public UI, public MetaDataUI, public PathBuilder {
public:
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

    const std::vector<std::string> &GetNames(const FaustParam &param) const { return NamesAndValues.at(param.zone).names; }
    const NamesAndValues &GetNamesAndValues(const FaustParam &param) const { return NamesAndValues.at(param.zone); }

    FaustParam RootParam{};

private:
    void Add(const FaustParam::Type type, const char *label, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        ActiveGroup().children.emplace_back(type, label, zone, min, max, init, step, fTooltip.contains(zone) ? fTooltip.at(zone).c_str() : nullptr);
        fFullPaths.push_back(buildPath(label));
    }

    FaustParam &ActiveGroup() { return Groups.empty() ? RootParam : *Groups.top(); }

    std::stack<FaustParam *> Groups{};
    std::unordered_map<const Real *, NamesAndValues> NamesAndValues;
};
