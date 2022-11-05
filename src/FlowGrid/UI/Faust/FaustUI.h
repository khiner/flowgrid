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

using std::string, std::vector, std::map;
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
        ItemType_Menu,
        ItemType_VRadioButtons,
        ItemType_HRadioButtons,
    };

    struct Item {
        Item(const ItemType type, const string &label, Real *zone = nullptr, Real min = 0, Real max = 0, Real init = 0, Real step = 0, const char *tooltip = nullptr, vector<Item> items = {})
            : type(type), id(label), label(label == "0x00" ? "" : label), zone(zone), min(min), max(max), init(init), step(step), tooltip(tooltip), items(std::move(items)) {}

        const ItemType type{ItemType_None};
        const string id, label; // `id` will be the same as `label` unless it's the special empty group label of '0x00', in which case `label` will be empty.
        Real *zone; // Only meaningful for widget items (not container items)
        const Real min, max; // Only meaningful for sliders, num-entries, and bar graphs.
        const Real init, step; // Only meaningful for sliders and num-entries.
        const char *tooltip;
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
        addSlider(label, zone, init, min, max, step, false);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        addSlider(label, zone, init, min, max, step, true);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_ui_item(ItemType_NumEntry, label, zone, min, max, init, step);
    }
    void addSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step, bool is_vertical) {
        if (isKnob(zone)) add_ui_item(ItemType_Knob, label, zone, min, max, init, step);
        else if (isRadio(zone)) addRadioButtons(label, zone, init, min, max, step, fRadioDescription[zone].c_str(), is_vertical);
        else if (isMenu(zone)) addMenu(label, zone, init, min, max, step, fMenuDescription[zone].c_str());
        else add_ui_item(is_vertical ? ItemType_VSlider : ItemType_HSlider, label, zone, min, max, init, step);
    }
    void addRadioButtons(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text, bool is_vertical) {
        names_and_values[zone] = {};
        parseMenuList(text, names_and_values[zone].names, names_and_values[zone].values);
        add_ui_item(is_vertical ? ItemType_VRadioButtons : ItemType_HRadioButtons, label, zone, min, max, init, step);
    }
    void addMenu(const char *label, Real *zone, Real init, Real min, Real max, Real step, const char *text) {
        names_and_values[zone] = {};
        parseMenuList(text, names_and_values[zone].names, names_and_values[zone].values);
        add_ui_item(ItemType_Menu, label, zone, min, max, init, step);
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

    Item *get_widget(const string &id) {
        if (index_for_path.contains(id)) return &ui.items[index_for_path[id]];
        if (index_for_shortname.contains(id)) return &ui.items[index_for_shortname[id]];
        if (index_for_label.contains(id)) return &ui.items[index_for_label[id]];

        return nullptr;
    }

    Item ui{ItemType_None, ""};
    map<const Real *, NamesAndValues> names_and_values;

private:
    void add_ui_item(const ItemType type, const string &label, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        active_group().items.emplace_back(type, label, zone, min, max, init, step, fTooltip.contains(zone) ? fTooltip.at(zone).c_str() : nullptr);
        const int index = int(ui.items.size() - 1);
        string path = buildPath(label);
        fFullPaths.push_back(path);
        index_for_path[path] = index;
        index_for_label[label] = index;
    }

    Item &active_group() { return groups.empty() ? ui : *groups.top(); }

    std::stack<Item *> groups{};
    map<string, int> index_for_label{};
    map<string, int> index_for_shortname{};
    map<string, int> index_for_path{};
};

class CTree;
typedef CTree *Box;
void on_ui_change(FaustUI *);
void on_box_change(Box);
void save_box_svg(const string &path);
