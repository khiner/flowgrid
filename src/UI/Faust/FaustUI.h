#pragma once

#include <vector>
#include <map>
#include <stack>
#include <string>
#include <iostream>

#include "../../Helper/Sample.h" // Must be included before any Faust includes
#include "faust/gui/UI.h"
#include "faust/gui/MetaDataUI.h"
#include "faust/gui/PathBuilder.h"

using std::string, std::vector;
using Real = Sample;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).

class FAUST_API FaustUI : public UI, public MetaDataUI, public PathBuilder {
public:
    FaustUI() = default;
    ~FaustUI() override = default;

    enum ItemType {
        ItemType_None = 0,
        // Containers
        ItemType_HGroup,
        ItemType_VGroup,
        ItemType_TGroup,

        // Widgets
        ItemType_Button,
        ItemType_CheckButton,
        ItemType_VSlider,
        ItemType_HSlider,
        ItemType_NumEntry,
        ItemType_HBargraph,
        ItemType_VBargraph,

        // Types specified with metadata
        ItemType_Knob,
        ItemType_VRadioButton,
        ItemType_HRadioButton,
    };

    struct Item {
        Item(const ItemType type, string label, Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, vector<Item> items = {})
            : type(type), label(std::move(label)), zone(zone), min(min), max(max), init(init), step(step), items(std::move(items)) {}

        const ItemType type{ItemType_None};
        const string label;
        Real *zone; // Only meaningful for widget items (not container items)
        const Real min, max; // Only meaningful for sliders, num-entries, and bar graphs.
        const Real init, step; // Only meaningful for sliders and num-entries.
        vector<Item> items; // Only populated for container items (groups)
    };
    struct NamesAndValues {
        vector<string> names{};
        vector<Real> values{};
    };

    void openHorizontalBox(const char *label) override {
        pushLabel(label);
        active_group().items.emplace_back(ItemType_HGroup, label);
        groups.push(&active_group().items.back());
    }
    void openVerticalBox(const char *label) override {
        pushLabel(label);
        active_group().items.emplace_back(ItemType_VGroup, label);
        groups.push(&active_group().items.back());
    }
    void openTabBox(const char *label) override {
        pushLabel(label);
        active_group().items.emplace_back(ItemType_TGroup, label);
        groups.push(&active_group().items.back());
    }
    void closeBox() override {
        groups.pop();
        if (popLabel()) {
            computeShortNames();
            for (const auto &it: fFullPaths) index_for_shortname[fFull2Short[it]] = index_for_path[it];
        }
    }

    // Active widgets
    void addButton(const char *label, Real *zone) override {
        add_ui_item(ItemType_Button, label, zone);
    }
    void addCheckButton(const char *label, Real *zone) override {
        add_ui_item(ItemType_CheckButton, label, zone);
    }
    void addHorizontalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        if (isRadio(zone)) {
            addRadioButtons(label, zone, *zone, min, max, step, fRadioDescription[zone].c_str(), false);
            return;
        }
        const auto type = isKnob(zone) ? ItemType_Knob : ItemType_HSlider;
        add_ui_item(type, label, zone, min, max, init, step);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        if (isRadio(zone)) {
            addRadioButtons(label, zone, *zone, min, max, step, fRadioDescription[zone].c_str(), true);
            return;
        }
        const auto type = isKnob(zone) ? ItemType_Knob : ItemType_VSlider;
        add_ui_item(type, label, zone, min, max, init, step);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_ui_item(ItemType_NumEntry, label, zone, min, max, init, step);
    }
    void addRadioButtons(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *description, bool is_vertical) {
        NamesAndValues names_and_values{};
        parseMenuList(description, names_and_values.names, names_and_values.values);
        radio_names_and_values[zone] = std::move(names_and_values);

        add_ui_item(is_vertical ? ItemType_VRadioButton : ItemType_HRadioButton, label, zone, min, max, init, step);
    }

    // Passive widgets
    void addHorizontalBargraph(const char *label, Real *zone, Real min, Real max) override {
        add_ui_item(ItemType_HBargraph, label, zone, min, max);
    }
    void addVerticalBargraph(const char *label, Real *zone, Real min, Real max) override {
        add_ui_item(ItemType_VBargraph, label, zone, min, max);
    }

    // Soundfile
    void addSoundfile(const char *, const char *, Soundfile **) override {}

    // Metadata declaration
    void declare(Real *zone, const char *key, const char *value) override {
        MetaDataUI::declare(zone, key, value);
    }

    // `id` can be any of label/shortname/path.
    Real get(const string &id) {
        const auto *widget = get_widget(id);
        if (!widget) {
            std::cerr << "ERROR : FaustUI::set : id " << id << " not found\n";
            return 0;
        }
        return *widget->zone;
    }

    // `id` can be any of label/shortname/path.
    void set(const string &id, Real value) {
        const auto *widget = get_widget(id);
        if (!widget) {
            std::cerr << "ERROR : FaustUI::set : id " << id << " not found\n";
            return;
        }
        *widget->zone = value;
    }

    Item *get_widget(const string &id) {
        if (index_for_path.contains(id)) return &ui.items[index_for_path[id]];
        if (index_for_shortname.contains(id)) return &ui.items[index_for_shortname[id]];
        if (index_for_label.contains(id)) return &ui.items[index_for_label[id]];

        return nullptr;
    }

    Item ui{ItemType_None, ""};
    std::map<const Real *, NamesAndValues> radio_names_and_values;

private:
    void add_ui_item(const ItemType type, const string &label, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        active_group().items.emplace_back(type, label, zone, min, max, init, step);
        const int index = int(ui.items.size() - 1);
        string path = buildPath(label);
        fFullPaths.push_back(path);
        index_for_path[path] = index;
        index_for_label[label] = index;
    }

    Item &active_group() { return groups.empty() ? ui : *groups.top(); }

    std::stack<Item *> groups{};
    std::map<string, int> index_for_label{};
    std::map<string, int> index_for_shortname{};
    std::map<string, int> index_for_path{};
};

class CTree;
typedef CTree *Box;
void on_ui_change(FaustUI *);
void on_box_change(Box);
void save_box_svg(const string &path);
