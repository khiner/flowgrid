#pragma once

#include "Device.h"

#include <set>
#include <string>
#include <vector>

using namespace std;

const double dWire = 8; // distance between two wires
const double dLetter = 4.3; // width of a letter
const double dHorz = 4;
const double dVert = 4;

struct point {
    double x;
    double y;

    point() : x(0.0), y(0.0) {}
    point(double u, double v) : x(u), y(v) {}

    bool operator<(const point &p) const {
        if (x < p.x) return true;
        if (x > p.x) return false;
        if (y < p.y) return true;
        return false;
    }
};

struct trait {
    point start, end;

    trait(const point &p1, const point &p2) : start(p1), end(p2) {}
    void draw(device &dev) const { dev.trait(start.x, start.y, end.x, end.y); }

    bool operator<(const trait &t) const {
        if (start < t.start) return true;
        if (t.start < start) return false;
        if (end < t.end) return true;
        return false;
    }
};

struct collector {
    set<point> outputs;     // collect real outputs
    set<point> inputs;      // collect real inputs
    set<trait> traits;      // collect traits to draw
    set<trait> withInput;   // collect traits with a real input
    set<trait> withOutput;  // collect traits with a real output

    void addOutput(const point &p) { outputs.insert(p); }
    void addInput(const point &p) { inputs.insert(p); }
    void addTrait(const trait &t) { traits.insert(t); }
    void computeVisibleTraits();
    bool isVisible(const trait &t);
    void draw(device &dev);
};

enum { kLeftRight = 1, kRightLeft = -1 };

/**
 * An abstract block diagram schema
 */
struct schema {
    const unsigned int inputs, outputs;
    const double width, height;

    // fields only defined after `place()` is called
    bool fPlaced; // `false` until `place()` is called
    double x, y;
    int orientation;

    schema(unsigned int inputs, unsigned int outputs, double width, double height)
        : inputs(inputs), outputs(outputs), width(width), height(height), fPlaced(false), x(0), y(0), orientation(0) {}
    virtual ~schema() = default;

    // starts and end placement
    void beginPlace(double new_x, double new_y, int new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
    }
    void endPlace() { fPlaced = true; }

    // fields available after placement
    bool placed() const { return fPlaced; }

    // abstract interface for subclasses
    virtual void place(double x, double y, int orientation) = 0;
    virtual void draw(device &dev) = 0;
    virtual point inputPoint(unsigned int i) const = 0;
    virtual point outputPoint(unsigned int i) const = 0;
    virtual void collectTraits(collector &c) = 0;
};

// various functions to create schemas

schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &name, const string &color, const string &link);
schema *makeCableSchema(unsigned int n = 1);
schema *makeInverterSchema(const string &color);
schema *makeCutSchema();
schema *makeEnlargedSchema(schema *s, double width);
schema *makeParSchema(schema *s1, schema *s2);
schema *makeSeqSchema(schema *s1, schema *s2);
schema *makeMergeSchema(schema *s1, schema *s2);
schema *makeSplitSchema(schema *s1, schema *s2);
schema *makeRecSchema(schema *s1, schema *s2);
schema *makeTopSchema(schema *s1, double margin, const string &text, const string &link);
schema *makeDecorateSchema(schema *s1, double margin, const string &text);
schema *makeConnectorSchema();
schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const vector<int> &routes);
