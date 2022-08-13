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

struct Point {
    double x;
    double y;

    Point() : x(0.0), y(0.0) {}
    Point(double u, double v) : x(u), y(v) {}

    bool operator<(const Point &p) const {
        if (x < p.x) return true;
        if (x > p.x) return false;
        if (y < p.y) return true;
        return false;
    }
};

struct Trait {
    Point start, end;

    Trait(const Point &p1, const Point &p2) : start(p1), end(p2) {}
    void draw(device &dev) const { dev.trait(start.x, start.y, end.x, end.y); }

    bool operator<(const Trait &t) const {
        if (start < t.start) return true;
        if (t.start < start) return false;
        if (end < t.end) return true;
        return false;
    }
};

struct Collector {
    set<Point> outputs;     // collect real outputs
    set<Point> inputs;      // collect real inputs
    set<Trait> traits;      // collect traits to draw
    set<Trait> withInput;   // collect traits with a real input
    set<Trait> withOutput;  // collect traits with a real output

    void addOutput(const Point &p) { outputs.insert(p); }
    void addInput(const Point &p) { inputs.insert(p); }
    void addTrait(const Trait &t) { traits.insert(t); }
    void computeVisibleTraits();
    bool isVisible(const Trait &t) const;
    void draw(device &dev);
};

enum { kLeftRight = 1, kRightLeft = -1 };

/**
 * An abstract block diagram schema
 */
struct Schema {
    const unsigned int inputs, outputs;
    const double width, height;

    // fields only defined after `beginPlace()` is called
    double x, y;
    int orientation;

    Schema(unsigned int inputs, unsigned int outputs, double width, double height)
        : inputs(inputs), outputs(outputs), width(width), height(height), x(0), y(0), orientation(0) {}
    virtual ~Schema() = default;

    void beginPlace(double new_x, double new_y, int new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
    }

    // abstract interface for subclasses
    virtual void place(double x, double y, int orientation) = 0;
    virtual void draw(device &dev) = 0;
    virtual Point inputPoint(unsigned int i) const = 0;
    virtual Point outputPoint(unsigned int i) const = 0;
    virtual void collectTraits(Collector &c) = 0;
};

Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &name, const string &color, const string &link);
Schema *makeCableSchema(unsigned int n = 1);
Schema *makeInverterSchema(const string &color);
Schema *makeCutSchema();
Schema *makeEnlargedSchema(Schema *s, double width);
Schema *makeParSchema(Schema *s1, Schema *s2);
Schema *makeSeqSchema(Schema *s1, Schema *s2);
Schema *makeMergeSchema(Schema *s1, Schema *s2);
Schema *makeSplitSchema(Schema *s1, Schema *s2);
Schema *makeRecSchema(Schema *s1, Schema *s2);
Schema *makeTopSchema(Schema *s1, double margin, const string &text, const string &link);
Schema *makeDecorateSchema(Schema *s1, double margin, const string &text);
Schema *makeConnectorSchema();
Schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const vector<int> &routes);
