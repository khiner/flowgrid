#pragma once

#include <vector>
#include <map>
#include <string>
#include <iostream>

#include "../../Helper/Sample.h" // Must be included before any Faust includes
#include "faust/gui/UI.h"
#include "faust/gui/PathBuilder.h"

using Real = Sample;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).

struct FAUST_API FaustUI : UI, PathBuilder {
    FaustUI() = default;
    ~FaustUI() override = default;

    enum WidgetType {
        WidgetType_Button = 0,
        WidgetType_CheckButton,
        WidgetType_VSlider,
        WidgetType_HSlider,
        WidgetType_NumEntry,
        WidgetType_HBargraph,
        WidgetType_VBargraph,
    };
    struct Widget {
        WidgetType type;
        Real *zone;
        Real min, max; // Only meaningful for sliders, num-entries, and bar graphs.
        Real init, step; // Only meaningful for sliders and num-entries.
    };

    void openTabBox(const char *label) override { pushLabel(label); }
    void openHorizontalBox(const char *label) override { pushLabel(label); }
    void openVerticalBox(const char *label) override { pushLabel(label); }
    void closeBox() override {
        if (popLabel()) {
            computeShortNames();
            for (const auto &it: fFullPaths) index_for_shortname[fFull2Short[it]] = index_for_path[it];
        }
    }

    // Active widgets
    void addButton(const char *label, Real *zone) override {
        add_widget(label, WidgetType_Button, zone);
    }
    void addCheckButton(const char *label, Real *zone) override {
        add_widget(label, WidgetType_CheckButton, zone);
    }
    void addHorizontalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_widget(label, WidgetType_HSlider, zone, min, max, init, step);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_widget(label, WidgetType_VSlider, zone, min, max, init, step);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_widget(label, WidgetType_NumEntry, zone, min, max, init, step);
    }

    // Passive widgets
    void addHorizontalBargraph(const char *label, Real *zone, Real min, Real max) override {
        add_widget(label, WidgetType_HBargraph, zone, min, max);
    }
    void addVerticalBargraph(const char *label, Real *zone, Real min, Real max) override {
        add_widget(label, WidgetType_VBargraph, zone, min, max);
    }

    // Soundfile
    void addSoundfile(const char *label, const char *filename, Soundfile **sf_zone) override {}

    // Metadata declaration
    void declare(Real *zone, const char *key, const char *val) override {}

    // `id` can be any of label/shortname/path.
    Real get(const std::string &id) {
        const auto *widget = get_widget(id);
        if (!widget) {
            std::cerr << "ERROR : FaustUI::set : id " << id << " not found\n";
            return 0;
        }
        return *widget->zone;
    }

    // `id` can be any of label/shortname/path.
    void set(const std::string &id, Real value) {
        const auto *widget = get_widget(id);
        if (!widget) {
            std::cerr << "ERROR : FaustUI::set : id " << id << " not found\n";
            return;
        }
        *widget->zone = value;
    }

    Widget *get_widget(const std::string &id) {
        if (index_for_path.contains(id)) return &widgets[index_for_path[id]];
        if (index_for_shortname.contains(id)) return &widgets[index_for_shortname[id]];
        if (index_for_label.contains(id)) return &widgets[index_for_label[id]];

        return nullptr;
    }

    std::vector<Widget> widgets;

private:
    void add_widget(const std::string &label, const WidgetType type, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        widgets.push_back({type, zone, min, max, init, step});
        const int index = int(widgets.size() - 1);
        std::string path = buildPath(label);
        fFullPaths.push_back(path);
        index_for_path[path] = index;
        index_for_label[label] = index;
    }

    std::map<std::string, int> index_for_label;
    std::map<std::string, int> index_for_shortname;
    std::map<std::string, int> index_for_path;
};
