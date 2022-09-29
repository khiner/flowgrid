#pragma once

#include <vector>
#include <map>
#include <stack>
#include <string>
#include <iostream>

#include "../../Helper/Sample.h" // Must be included before any Faust includes
#include "faust/gui/UI.h"
#include "faust/gui/PathBuilder.h"

using Real = Sample;

// Label, shortname, or complete path (to discriminate between possibly identical labels
// at different locations in the UI hierarchy) can be used to access any created widget.
// See Faust's `APIUI` for possible extensions (response curves, gyro, ...).

class FAUST_API FaustUI : public UI, public PathBuilder {
public:
    FaustUI() = default;
    ~FaustUI() override = default;

    enum ItemType {
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
    };
    struct Item {
        const ItemType type{ItemType_VGroup};
        const std::string label;
        Real *zone{nullptr}; // Only meaningful for widget items (not container items)
        const Real min{0}, max{0}; // Only meaningful for sliders, num-entries, and bar graphs.
        const Real init{0}, step{0}; // Only meaningful for sliders and num-entries.
        std::vector<Item> items{}; // Only populated for container items (groups)
    };

    void openHorizontalBox(const char *label) override {
        pushLabel(label);
        active_items().push_back({ItemType_HGroup, label});
        groups.push(&active_items().back());
    }
    void openVerticalBox(const char *label) override {
        pushLabel(label);
        active_items().push_back({ItemType_VGroup, label});
        groups.push(&active_items().back());
    }
    void openTabBox(const char *label) override {
        pushLabel(label);
        active_items().push_back({ItemType_TGroup, label});
        groups.push(&active_items().back());
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
        add_ui_item(ItemType_HSlider, label, zone, min, max, init, step);
    }
    void addVerticalSlider(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_ui_item(ItemType_VSlider, label, zone, min, max, init, step);
    }
    void addNumEntry(const char *label, Real *zone, Real init, Real min, Real max, Real step) override {
        add_ui_item(ItemType_NumEntry, label, zone, min, max, init, step);
    }

    // Passive widgets
    void addHorizontalBargraph(const char *label, Real *zone, Real min, Real max) override {
        add_ui_item(ItemType_HBargraph, label, zone, min, max);
    }
    void addVerticalBargraph(const char *label, Real *zone, Real min, Real max) override {
        add_ui_item(ItemType_VBargraph, label, zone, min, max);
    }

    // Soundfile
    void addSoundfile(const char *label, const char *filename, Soundfile **sf_zone) override {}

    // Metadata declaration
    void declare(Real *zone, const char *key, const char *val) override {
        cout << "declare: " << key << ": " << val << '\n';
    }

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

    Item *get_widget(const std::string &id) {
        if (index_for_path.contains(id)) return &ui[index_for_path[id]];
        if (index_for_shortname.contains(id)) return &ui[index_for_shortname[id]];
        if (index_for_label.contains(id)) return &ui[index_for_label[id]];

        return nullptr;
    }

    std::vector<Item> ui{};

private:
    void add_ui_item(const ItemType type, const std::string &label, Real *zone, Real min = 0, Real max = 0, Real init = 0, Real step = 0) {
        active_items().push_back({type, label, zone, min, max, init, step});

        const int index = int(ui.size() - 1);
        std::string path = buildPath(label);
        fFullPaths.push_back(path);
        index_for_path[path] = index;
        index_for_label[label] = index;
    }

    std::vector<Item> &active_items() { return groups.empty() ? ui : groups.top()->items; }

    std::stack<Item *> groups{};
    std::map<std::string, int> index_for_label{};
    std::map<std::string, int> index_for_shortname{};
    std::map<std::string, int> index_for_path{};
};
