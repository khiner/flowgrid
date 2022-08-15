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
    double x, y;

    Point() : x(0.0), y(0.0) {}
    Point(double x, double y) : x(x), y(y) {}

    bool operator<(const Point &p) const {
        if (x < p.x) return true;
        if (x > p.x) return false;
        if (y < p.y) return true;
        return false;
    }
};

struct Line {
    Point start, end;

    Line(const Point &p1, const Point &p2) : start(p1), end(p2) {}
    void draw(Device &device) const { device.line(start.x, start.y, end.x, end.y); }

    bool operator<(const Line &t) const {
        if (start < t.start) return true;
        if (t.start < start) return false;
        if (end < t.end) return true;
        return false;
    }
};

struct Collector {
    set<Point> outputs; // collect real outputs
    set<Point> inputs; // collect real inputs
    set<Line> lines; // collect lines to draw
    set<Line> withInput; // collect lines with a real input
    set<Line> withOutput; // collect lines with a real output

    void draw(Device &);

private:
    bool computeVisibleLines();
};

// An abstract block diagram schema
struct Schema {
    const unsigned int inputs, outputs;
    const double width, height;

    // Fields only defined after `place()` is called:
    double x = 0, y = 0;
    int orientation = 0;

    Schema(unsigned int inputs, unsigned int outputs, double width, double height) : inputs(inputs), outputs(outputs), width(width), height(height) {}
    virtual ~Schema() = default;

    void place(double new_x, double new_y, int new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        placeImpl();
    }

    // abstract interface for subclasses
    virtual void placeImpl() = 0;
    virtual void draw(Device &) = 0;
    virtual Point inputPoint(unsigned int i) const = 0;
    virtual Point outputPoint(unsigned int i) const = 0;
    virtual void collectLines(Collector &c) = 0;
};

struct IOSchema : Schema {
    IOSchema(unsigned int inputs, unsigned int outputs, double width, double height) : Schema(inputs, outputs, width, height) {
        for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
        for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
    }

    void placeImpl() override {
        const bool isLR = orientation == kLeftRight;
        const double dir = isLR ? -1.0 : 1.0;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = {isLR ? x : x + width, y + height / 2.0 - dWire * ((inputs - 1) / 2.0 + i * dir)};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = {isLR ? x + width : x, y + height / 2.0 - dWire * ((outputs - 1) / 2.0 + i * dir)};
    }

    Point inputPoint(unsigned int i) const override { return inputPoints[i]; }
    Point outputPoint(unsigned int i) const override { return outputPoints[i]; }

    vector<Point> inputPoints;
    vector<Point> outputPoints;
};
struct BinarySchema : Schema {
    BinarySchema(Schema *s1, Schema *s2, unsigned int inputs, unsigned int outputs, double width, double height)
        : Schema(inputs, outputs, width, height), schema1(s1), schema2(s2) {}

    Point inputPoint(unsigned int i) const override { return schema1->inputPoint(i); }
    Point outputPoint(unsigned int i) const override { return schema2->outputPoint(i); }

    void draw(Device &device) override {
        schema1->draw(device);
        schema2->draw(device);
    }

    void collectLines(Collector &c) override {
        schema1->collectLines(c);
        schema2->collectLines(c);
    }

    Schema *schema1;
    Schema *schema2;
};

Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link);
Schema *makeCableSchema(unsigned int n = 1);
Schema *makeInverterSchema(const string &color);
Schema *makeCutSchema();
Schema *makeEnlargedSchema(Schema *s, double width);
Schema *makeParallelSchema(Schema *s1, Schema *s2);
Schema *makeSequentialSchema(Schema *s1, Schema *s2);
Schema *makeMergeSchema(Schema *s1, Schema *s2);
Schema *makeSplitSchema(Schema *s1, Schema *s2);
Schema *makeRecSchema(Schema *s1, Schema *s2);
Schema *makeTopSchema(Schema *s1, double margin, const string &text, const string &link);
Schema *makeDecorateSchema(Schema *s1, double margin, const string &text);
Schema *makeConnectorSchema();
Schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const vector<int> &routes);
