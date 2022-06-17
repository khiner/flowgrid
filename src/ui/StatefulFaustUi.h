#pragma once

// Adapted from `faust/gui/APIUI.h`

#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <map>

#include "faust/gui/meta.h"
#include "faust/gui/UI.h"
#include "faust/gui/PathBuilder.h"
#include "faust/gui/ValueConverter.h"

#include "../context.h"

using uint = unsigned int;

struct StatefulFaustUI : PathBuilder, Meta, UI {
    enum ItemType { kButton = 0, kCheckButton, kVSlider, kHSlider, kNumEntry, kHBargraph, kVBargraph };
    enum Type { kAcc = 0, kGyr = 1, kNoType };

public:
    StatefulFaustUI() : has_screen_control(false), red_reader(nullptr), green_reader(nullptr), blue_reader(nullptr), current_scale(kLin) {}

    ~StatefulFaustUI() override {
        for (const auto &it: items) delete it.value_converter;
        for (int i = 0; i < 3; i++) {
            for (const auto &it: acc[i]) delete it;
            for (const auto &it: gyr[i]) delete it;
        }
        delete red_reader;
        delete green_reader;
        delete blue_reader;
    }

    // -- widget's layouts

    void openTabBox(const char *label) override { pushLabel(label); }
    void openHorizontalBox(const char *label) override { pushLabel(label); }
    void openVerticalBox(const char *label) override { pushLabel(label); }
    void closeBox() override {
        if (popLabel()) {
            // Shortnames can be computed when all fullnames are known
            computeShortNames();
            // Fill 'shortname' field for each item
            for (const auto &it: fFull2Short) {
                int index = getParamIndex(it.first.c_str());
                items[index].shortname = it.second;
            }
        }
    }

    // -- active widgets

    void addButton(const char *label, FAUSTFLOAT *zone) override {
        addParameter(label, zone, 0, 0, 1, 1, kButton);
    }

    void addCheckButton(const char *label, FAUSTFLOAT *zone) override {
        addParameter(label, zone, 0, 0, 1, 1, kCheckButton);
    }

    void addVerticalSlider(const char *label, FAUSTFLOAT *zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override {
        addParameter(label, zone, init, min, max, step, kVSlider);
    }

    void addHorizontalSlider(const char *label, FAUSTFLOAT *zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override {
        addParameter(label, zone, init, min, max, step, kHSlider);
    }

    void addNumEntry(const char *label, FAUSTFLOAT *zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step) override {
        addParameter(label, zone, init, min, max, step, kNumEntry);
    }

    // -- passive widgets

    void addHorizontalBargraph(const char *label, FAUSTFLOAT *zone, FAUSTFLOAT min, FAUSTFLOAT max) override {
        addParameter(label, zone, min, min, max, (max - min) / 1000.0f, kHBargraph);
    }

    void addVerticalBargraph(const char *label, FAUSTFLOAT *zone, FAUSTFLOAT min, FAUSTFLOAT max) override {
        addParameter(label, zone, min, min, max, (max - min) / 1000.0f, kVBargraph);
    }

    // -- soundfiles

    void addSoundfile(const char *label, const char *filename, Soundfile **sf_zone) override {}

    // -- metadata declarations

    void declare(FAUSTFLOAT *zone, const char *key, const char *val) override {
        // Keep metadata
        current_metadata[key] = val;

        if (strcmp(key, "scale") == 0) {
            current_scale = strcmp(val, "log") == 0 ? kLog : strcmp(val, "exp") == 0 ? kExp : kLin;
        } else if (strcmp(key, "unit") == 0) {
            current_unit = val;
        } else if (strcmp(key, "acc") == 0) {
            current_acc = val;
        } else if (strcmp(key, "gyr") == 0) {
            current_gyr = val;
        } else if (strcmp(key, "screencolor") == 0) {
            current_color = val; // val = "red", "green", "blue" or "white"
        } else if (strcmp(key, "tooltip") == 0) {
            current_tooltip = val;
        }
    }

    void declare(const char *key, const char *val) override {}

    //-------------------------------------------------------------------------------
    // Simple API part
    //-------------------------------------------------------------------------------
    int getParamsCount() { return int(items.size()); }

    int getParamIndex(const char *path_aux) {
        string path = string(path_aux);
        auto it = find_if(items.begin(), items.end(),
            [=](const Item &it) { return (it.label == path) || (it.shortname == path) || (it.path == path); });
        return (it != items.end()) ? int(it - items.begin()) : -1;
    }

    const char *getParamLabel(int p) { return items[uint(p)].label.c_str(); }
    const char *getParamShortname(int p) { return items[uint(p)].shortname.c_str(); }
    const char *getParamAddress(int p) { return items[uint(p)].path.c_str(); }

    std::map<const char *, const char *> getMetadata(int p) {
        std::map<const char *, const char *> res;
        std::map<string, string> _metadata = metadata[uint(p)];
        for (const auto &it: _metadata) {
            res[it.first.c_str()] = it.second.c_str();
        }
        return res;
    }

    const char *getMetadata(int p, const char *key) {
        return (metadata[uint(p)].find(key) != metadata[uint(p)].end()) ? metadata[uint(p)][key].c_str() : "";
    }
    FAUSTFLOAT getParamMin(int p) { return items[uint(p)].min; }
    FAUSTFLOAT getParamMax(int p) { return items[uint(p)].max; }
    FAUSTFLOAT getParamStep(int p) { return items[uint(p)].step; }
    FAUSTFLOAT getParamInit(int p) { return items[uint(p)].init; }

    FAUSTFLOAT *getParamZone(int p) { return items[uint(p)].zone; }

    FAUSTFLOAT getParamValue(int p) { return *items[uint(p)].zone; }
    FAUSTFLOAT getParamValue(const char *path) {
        int index = getParamIndex(path);
        if (index >= 0) return getParamValue(index);

        fprintf(stderr, "getParamValue : '%s' not found\n", (path == nullptr ? "NULL" : path));
        return FAUSTFLOAT(0);
    }

    void setParamValue(int p, FAUSTFLOAT v) {
        *items[uint(p)].zone = v;
    }
    void setParamValue(const char *path, FAUSTFLOAT v) {
        int index = getParamIndex(path);
        if (index >= 0) {
            setParamValue(index, v);
        } else {
            fprintf(stderr, "setParamValue : '%s' not found\n", (path == nullptr ? "NULL" : path));
        }
    }

    double getParamRatio(int p) { return items[uint(p)].value_converter->faust2ui(*items[uint(p)].zone); }
    void setParamRatio(int p, double r) { *items[uint(p)].zone = FAUSTFLOAT(items[uint(p)].value_converter->ui2faust(r)); }

    double value2ratio(int p, double r) { return items[uint(p)].value_converter->faust2ui(r); }
    double ratio2value(int p, double r) { return items[uint(p)].value_converter->ui2faust(r); }

    /**
     * Return the control type (kAcc, kGyr, or -1) for a given parameter.
     *
     * @param p - the UI parameter index
     *
     * @return the type
     */
    Type getParamType(int p) {
        if (p >= 0) {
            if (getZoneIndex(acc, p, 0) != -1
                || getZoneIndex(acc, p, 1) != -1
                || getZoneIndex(acc, p, 2) != -1) {
                return kAcc;
            }
            if (getZoneIndex(gyr, p, 0) != -1
                || getZoneIndex(gyr, p, 1) != -1
                || getZoneIndex(gyr, p, 2) != -1) {
                return kGyr;
            }
        }
        return kNoType;
    }

    /**
     * Return the Item type (kButton = 0, kCheckButton, kVSlider, kHSlider, kNumEntry, kHBargraph, kVBargraph) for a given parameter.
     *
     * @param p - the UI parameter index
     *
     * @return the Item type
     */
    ItemType getParamItemType(int p) {
        return items[uint(p)].item_type;
    }

    /**
     * Set a new value coming from an accelerometer, propagate it to all relevant FAUSTFLOAT* zones.
     *
     * @param acc - 0 for X accelerometer, 1 for Y accelerometer, 2 for Z accelerometer
     * @param value - the new value
     *
     */
    void propagateAcc(int _acc, double value) {
        for (auto &i: acc[_acc]) {
            i->update(value);
        }
    }

    /**
     * Used to edit accelerometer curves and mapping. Set curve and related mapping for a given UI parameter.
     *
     * @param p - the UI parameter index
     * @param acc - 0 for X accelerometer, 1 for Y accelerometer, 2 for Z accelerometer (-1 means "no mapping")
     * @param curve - between 0 and 3
     * @param amin - mapping 'min' point
     * @param amid - mapping 'middle' point
     * @param amax - mapping 'max' point
     *
     */
    void setAccConverter(int p, int _acc, int curve, double amin, double amid, double amax) {
        setConverter(acc, p, _acc, curve, amin, amid, amax);
    }

    /**
     * Used to edit gyroscope curves and mapping. Set curve and related mapping for a given UI parameter.
     *
     * @param p - the UI parameter index
     * @param gry - 0 for X gyroscope, 1 for Y gyroscope, 2 for Z gyroscope (-1 means "no mapping")
     * @param curve - between 0 and 3
     * @param amin - mapping 'min' point
     * @param amid - mapping 'middle' point
     * @param amax - mapping 'max' point
     *
     */
    void setGyrConverter(int p, int _gyr, int curve, double amin, double amid, double amax) {
        setConverter(gyr, p, _gyr, curve, amin, amid, amax);
    }

    /**
     * Used to edit accelerometer curves and mapping. Get curve and related mapping for a given UI parameter.
     *
     * @param p - the UI parameter index
     * @param acc - the acc value to be retrieved (-1 means "no mapping")
     * @param curve - the curve value to be retrieved
     * @param amin - the amin value to be retrieved
     * @param amid - the amid value to be retrieved
     * @param amax - the amax value to be retrieved
     *
     */
    void getAccConverter(int p, int &_acc, int &curve, double &amin, double &amid, double &amax) {
        getConverter(acc, p, _acc, curve, amin, amid, amax);
    }

    /**
     * Used to edit gyroscope curves and mapping. Get curve and related mapping for a given UI parameter.
     *
     * @param p - the UI parameter index
     * @param gyr - the gyr value to be retrieved (-1 means "no mapping")
     * @param curve - the curve value to be retrieved
     * @param amin - the amin value to be retrieved
     * @param amid - the amid value to be retrieved
     * @param amax - the amax value to be retrieved
     *
     */
    void getGyrConverter(int p, int &_gyr, int &curve, double &amin, double &amid, double &amax) {
        getConverter(gyr, p, _gyr, curve, amin, amid, amax);
    }

    /**
     * Set a new value coming from an gyroscope, propagate it to all relevant FAUSTFLOAT* zones.
     *
     * @param gyr - 0 for X gyroscope, 1 for Y gyroscope, 2 for Z gyroscope
     * @param value - the new value
     *
     */
    void propagateGyr(int _gyr, double value) {
        for (auto &i: gyr[_gyr]) {
            i->update(value);
        }
    }

    /**
     * Get the number of FAUSTFLOAT* zones controlled with the accelerometer.
     *
     * @param acc - 0 for X accelerometer, 1 for Y accelerometer, 2 for Z accelerometer
     * @return the number of zones
     *
     */
    int getAccCount(int _acc) {
        return (_acc >= 0 && _acc < 3) ? int(acc[_acc].size()) : 0;
    }

    /**
     * Get the number of FAUSTFLOAT* zones controlled with the gyroscope.
     *
     * @param gyr - 0 for X gyroscope, 1 for Y gyroscope, 2 for Z gyroscope
     * @param the number of zones
     *
     */
    int getGyrCount(int _gyr) {
        return (_gyr >= 0 && _gyr < 3) ? int(gyr[_gyr].size()) : 0;
    }

    // getScreenColor() : -1 means no screen color control (no screencolor metadata found)
    // otherwise return 0x00RRGGBB a ready to use color
    int getScreenColor() {
        if (has_screen_control) {
            int r = (red_reader) ? red_reader->getValue() : 0;
            int g = (green_reader) ? green_reader->getValue() : 0;
            int b = (blue_reader) ? blue_reader->getValue() : 0;
            return (r << 16) | (g << 8) | b;
        }
        return -1;
    }

protected:
    enum Mapping { kLin = 0, kLog = 1, kExp = 2 };

    struct Item {
        string label;
        string shortname;
        string path;
        ValueConverter *value_converter;
        FAUSTFLOAT *zone;
        FAUSTFLOAT init;
        FAUSTFLOAT min;
        FAUSTFLOAT max;
        FAUSTFLOAT step;
        ItemType item_type;

        Item(const string &label,
             const string &short_name,
             const string &path,
             ValueConverter *value_converter,
             FAUSTFLOAT *zone,
             FAUSTFLOAT init,
             FAUSTFLOAT min,
             FAUSTFLOAT max,
             FAUSTFLOAT step,
             ItemType item_type)
            : label(label), shortname(short_name), path(path), value_converter(value_converter),
              zone(zone), init(init), min(min), max(max), step(step), item_type(item_type) {}
    };
    std::vector<Item> items;

    std::vector<std::map<string, string> > metadata;
    std::vector<ZoneControl *> acc[3];
    std::vector<ZoneControl *> gyr[3];

    // Screen color control
    // "...[screencolor:red]..." etc.
    bool has_screen_control;      // true if control screen color metadata
    ZoneReader *red_reader;
    ZoneReader *green_reader;
    ZoneReader *blue_reader;

    // Current values controlled by metadata
    string current_unit;
    int current_scale;
    string current_acc;
    string current_gyr;
    string current_color;
    string current_tooltip;
    std::map<string, string> current_metadata;

    // Add a generic parameter
    void addParameter(const char *label, FAUSTFLOAT *zone, FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step, ItemType type) {
        string path = buildPath(label);
        fFullPaths.push_back(path);

        // handle scale metadata
        ValueConverter *converter = nullptr;
        switch (current_scale) {
            case kLin:converter = new LinearValueConverter(0, 1, min, max);
                break;
            case kLog:converter = new LogValueConverter(0, 1, min, max);
                break;
            case kExp:converter = new ExpValueConverter(0, 1, min, max);
                break;
        }
        current_scale = kLin;

        items.push_back(Item(label, "", path, converter, zone, init, min, max, step, type));

        if (!current_acc.empty() && !current_gyr.empty()) {
            fprintf(stderr, "warning : 'acc' and 'gyr' metadata used for the same %s parameter !!\n", label);
        }

        // handle acc metadata "...[acc : <axe> <curve> <amin> <amid> <amax>]..."
        if (!current_acc.empty()) {
            std::istringstream iss(current_acc);
            int axe, curve;
            double amin, amid, amax;
            iss >> axe >> curve >> amin >> amid >> amax;

            if ((0 <= axe) && (axe < 3) &&
                (0 <= curve) && (curve < 4) &&
                (amin < amax) && (amin <= amid) && (amid <= amax)) {
                acc[axe].push_back(new CurveZoneControl(zone, curve, amin, amid, amax, min, init, max));
            } else {
                fprintf(stderr, "incorrect acc metadata : %s \n", current_acc.c_str());
            }
            current_acc = "";
        }

        // handle gyr metadata "...[gyr : <axe> <curve> <amin> <amid> <amax>]..."
        if (!current_gyr.empty()) {
            std::istringstream iss(current_gyr);
            int axe, curve;
            double amin, amid, amax;
            iss >> axe >> curve >> amin >> amid >> amax;

            if ((0 <= axe) && (axe < 3) &&
                (0 <= curve) && (curve < 4) &&
                (amin < amax) && (amin <= amid) && (amid <= amax)) {
                gyr[axe].push_back(new CurveZoneControl(zone, curve, amin, amid, amax, min, init, max));
            } else {
                fprintf(stderr, "incorrect gyr metadata : %s \n", current_gyr.c_str());
            }
            current_gyr = "";
        }

        // handle screencolor metadata "...[screencolor:red|green|blue|white]..."
        if (!current_color.empty()) {
            if ((current_color == "red") && (red_reader == nullptr)) {
                red_reader = new ZoneReader(zone, min, max);
                has_screen_control = true;
            } else if ((current_color == "green") && (green_reader == nullptr)) {
                green_reader = new ZoneReader(zone, min, max);
                has_screen_control = true;
            } else if ((current_color == "blue") && (blue_reader == nullptr)) {
                blue_reader = new ZoneReader(zone, min, max);
                has_screen_control = true;
            } else if ((current_color == "white") && (red_reader == nullptr) && (green_reader == nullptr) && (blue_reader == nullptr)) {
                red_reader = new ZoneReader(zone, min, max);
                green_reader = new ZoneReader(zone, min, max);
                blue_reader = new ZoneReader(zone, min, max);
                has_screen_control = true;
            } else {
                fprintf(stderr, "incorrect screencolor metadata : %s \n", current_color.c_str());
            }
        }
        current_color = "";

        metadata.push_back(current_metadata);
        current_metadata.clear();
    }

    int getZoneIndex(std::vector<ZoneControl *> *table, int p, int val) {
        FAUSTFLOAT *zone = items[uint(p)].zone;
        for (size_t i = 0; i < table[val].size(); i++) {
            if (zone == table[val][i]->getZone()) return int(i);
        }
        return -1;
    }

    void setConverter(std::vector<ZoneControl *> *table, int p, int val, int curve, double amin, double amid, double amax) {
        int id1 = getZoneIndex(table, p, 0);
        int id2 = getZoneIndex(table, p, 1);
        int id3 = getZoneIndex(table, p, 2);

        // Deactivates everywhere..
        if (id1 != -1) table[0][uint(id1)]->setActive(false);
        if (id2 != -1) table[1][uint(id2)]->setActive(false);
        if (id3 != -1) table[2][uint(id3)]->setActive(false);

        if (val == -1) { // Means: no more mapping...
            // So stay all deactivated...
        } else {
            int id4 = getZoneIndex(table, p, val);
            if (id4 != -1) {
                // Reactivate the one we edit...
                table[val][uint(id4)]->setMappingValues(curve, amin, amid, amax, items[uint(p)].min, items[uint(p)].init, items[uint(p)].max);
                table[val][uint(id4)]->setActive(true);
            } else {
                // Allocate a new CurveZoneControl which is 'active' by default
                FAUSTFLOAT *zone = items[uint(p)].zone;
                table[val].push_back(new CurveZoneControl(zone, curve, amin, amid, amax, items[uint(p)].min, items[uint(p)].init, items[uint(p)].max));
            }
        }
    }

    void getConverter(std::vector<ZoneControl *> *table, int p, int &val, int &curve, double &amin, double &amid, double &amax) {
        int id1 = getZoneIndex(table, p, 0);
        int id2 = getZoneIndex(table, p, 1);
        int id3 = getZoneIndex(table, p, 2);

        if (id1 != -1) {
            val = 0;
            curve = table[val][uint(id1)]->getCurve();
            table[val][uint(id1)]->getMappingValues(amin, amid, amax);
        } else if (id2 != -1) {
            val = 1;
            curve = table[val][uint(id2)]->getCurve();
            table[val][uint(id2)]->getMappingValues(amin, amid, amax);
        } else if (id3 != -1) {
            val = 2;
            curve = table[val][uint(id3)]->getCurve();
            table[val][uint(id3)]->getMappingValues(amin, amid, amax);
        } else {
            val = -1; // No mapping
            curve = 0;
            amin = -100.;
            amid = 0.;
            amax = 100.;
        }
    }
};
