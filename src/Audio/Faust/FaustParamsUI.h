#pragma once

#include "FaustParamType.h"
#include "FaustParamsContainer.h"

#include "faust/gui/MetaDataUI.h"
#include "faust/gui/PathBuilder.h"
#include "faust/gui/UI.h"

struct FaustParamsStyle;

class FaustParamsUI : public UI, public MetaDataUI, public PathBuilder {
public:
    FaustParamsUI(FaustParamsContainer &container) : Container(container) {}

    void openHorizontalBox(const char *label) override { Add(Type_HGroup, label); }
    void openVerticalBox(const char *label) override { Add(Type_VGroup, label); }
    void openTabBox(const char *label) override { Add(Type_TGroup, label); }

    void closeBox() override { PopGroup(); }

    // Active widgets
    void addButton(const char *label, Real *zone) override { Add(Type_Button, label, zone); }
    void addCheckButton(const char *label, Real *zone) override { Add(Type_CheckButton, label, zone); }
    void addHorizontalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, false);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, true);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        Add(Type_NumEntry, label, zone, min, max, init, step);
    }
    void addSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step, bool is_vertical) {
        if (isKnob(zone)) Add(Type_Knob, label, zone, min, max, init, step);
        else if (isRadio(zone)) addRadioButtons(label, zone, init, min, max, step, fRadioDescription[zone].c_str(), is_vertical);
        else if (isMenu(zone)) addMenu(label, zone, init, min, max, step, fMenuDescription[zone].c_str());
        else Add(is_vertical ? Type_VSlider : Type_HSlider, label, zone, min, max, init, step);
    }
    void addRadioButtons(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text, bool is_vertical) {
        NamesAndValues names_and_values;
        parseMenuList(text, names_and_values.names, names_and_values.values);
        Add(is_vertical ? Type_VRadioButtons : Type_HRadioButtons, label, zone, min, max, init, step, std::move(names_and_values));
    }
    void addMenu(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text) {
        NamesAndValues names_and_values;
        parseMenuList(text, names_and_values.names, names_and_values.values);
        Add(Type_Menu, label, zone, min, max, init, step, std::move(names_and_values));
    }

    // Passive widgets
    void addHorizontalBargraph(const char *label, Real *zone, Real min, Real max) override { Add(Type_HBargraph, label, zone, min, max); }
    void addVerticalBargraph(const char *label, Real *zone, Real min, Real max) override { Add(Type_VBargraph, label, zone, min, max); }

    // Soundfile
    void addSoundfile(const char *, const char *, Soundfile **) override {}

    // Metadata declaration
    void declare(Real *zone, const char *key, const char *value) override { MetaDataUI::declare(zone, key, value); }

    FaustParamsContainer &Container; // Forwarded to params.

private:
    void Add(FaustParamType type, const char *label, Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, NamesAndValues names_and_values = {}) {
        if (zone == nullptr) pushLabel(label);
        else addFullPath(label);

        // Replace char list copied from `PathBuilder::buildPath`.
        // The difference is this is only applied to the new label (leaf segment) rather than the full path.
        std::string short_label = replaceCharList(label, {' ', '#', '*', ',', '?', '[', ']', '{', '}', '(', ')'}, '_');
        Container.Add(type, label, short_label, zone, min, max, init, step, zone != nullptr && fTooltip.contains(zone) ? fTooltip.at(zone).c_str() : nullptr, std::move(names_and_values));
    }

    void PopGroup() {
        if (popLabel()) computeShortNames();

        Container.PopGroup();
    }
};
