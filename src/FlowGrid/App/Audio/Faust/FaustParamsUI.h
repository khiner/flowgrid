#pragma once

#include <iostream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "faust/gui/MetaDataUI.h"
#include "faust/gui/PathBuilder.h"
#include "faust/gui/UI.h"

#include "FaustParam.h"
#include "UI/NamesAndValues.h"

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).
class FAUST_API FaustParamsUI : public UI, public MetaDataUI, public PathBuilder {
public:
    FaustParamsUI() = default;
    ~FaustParamsUI() override = default;

    void openHorizontalBox(const char *label) override {
        pushLabel(label);
        activeGroup().children.emplace_back(FaustParam::Type_HGroup, label);
        Groups.push(&activeGroup().children.back());
    }
    void openVerticalBox(const char *label) override {
        pushLabel(label);
        activeGroup().children.emplace_back(FaustParam::Type_VGroup, label);
        Groups.push(&activeGroup().children.back());
    }
    void openTabBox(const char *label) override {
        pushLabel(label);
        activeGroup().children.emplace_back(FaustParam::Type_TGroup, label);
        Groups.push(&activeGroup().children.back());
    }
    void closeBox() override {
        Groups.pop();
        if (popLabel()) {
            computeShortNames();
        }
    }

    // Active widgets
    void addButton(const char *label, Real *zone) override {
        addUiItem(FaustParam::Type_Button, label, zone);
    }
    void addCheckButton(const char *label, Real *zone) override {
        addUiItem(FaustParam::Type_CheckButton, label, zone);
    }
    void addHorizontalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, false);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, true);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addUiItem(FaustParam::Type_NumEntry, label, zone, min, max, init, step);
    }
    void addSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step, bool is_vertical) {
        if (isKnob(zone)) addUiItem(FaustParam::Type_Knob, label, zone, min, max, init, step);
        else if (isRadio(zone)) addRadioButtons(label, zone, init, min, max, step, fRadioDescription[zone].c_str(), is_vertical);
        else if (isMenu(zone)) addMenu(label, zone, init, min, max, step, fMenuDescription[zone].c_str());
        else addUiItem(is_vertical ? FaustParam::Type_VSlider : FaustParam::Type_HSlider, label, zone, min, max, init, step);
    }
    void addRadioButtons(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text, bool is_vertical) {
        NamesAndValues[zone] = {};
        parseMenuList(text, NamesAndValues[zone].names, NamesAndValues[zone].values);
        addUiItem(is_vertical ? FaustParam::Type_VRadioButtons : FaustParam::Type_HRadioButtons, label, zone, min, max, init, step);
    }
    void addMenu(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text) {
        NamesAndValues[zone] = {};
        parseMenuList(text, NamesAndValues[zone].names, NamesAndValues[zone].values);
        addUiItem(FaustParam::Type_Menu, label, zone, min, max, init, step);
    }

    // Passive widgets
    void addHorizontalBargraph(const char *label, Real *zone, Real min, Real max) override {
        addUiItem(FaustParam::Type_HBargraph, label, zone, min, max);
    }
    void addVerticalBargraph(const char *label, Real *zone, Real min, Real max) override {
        addUiItem(FaustParam::Type_VBargraph, label, zone, min, max);
    }

    // Soundfile
    void addSoundfile(const char *, const char *, Soundfile **) override {}

    // Metadata declaration
    void declare(Real *zone, const char *key, const char *value) override {
        MetaDataUI::declare(zone, key, value);
    }
    FaustParam UiParam{FaustParam::Type_None, ""};
    std::unordered_map<const Real *, NamesAndValues> NamesAndValues;

private:
    void addUiItem(const FaustParam::Type type, const char *label, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        activeGroup().children.emplace_back(type, label, zone, min, max, init, step, fTooltip.contains(zone) ? fTooltip.at(zone).c_str() : nullptr);
        const std::string path = buildPath(label);
        fFullPaths.push_back(path);
    }

    FaustParam &activeGroup() { return Groups.empty() ? UiParam : *Groups.top(); }

    std::stack<FaustParam *> Groups{};
};

void OnUiChange(FaustParamsUI *);
