#pragma once

#include "Device.h"

#include <vector>

const float dWire = 8; // distance between two wires
const float dLetter = 4.3; // width of a letter
const float dHorz = 4;
const float dVert = 4;

// An abstract block diagram schema
struct Schema {
    const unsigned int inputs, outputs;
    const float width, height;

    // Fields only defined after `place()` is called:
    float x = 0, y = 0;
    int orientation = 0;

    std::vector<Line> lines; // Populated in `collectLines()`

    Schema(unsigned int inputs, unsigned int outputs, float width, float height) : inputs(inputs), outputs(outputs), width(width), height(height) {}
    virtual ~Schema() = default;

    void place(float new_x, float new_y, int new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        placeImpl();
    }

    void draw(Device &device) const {
        for (const auto &line: lines) { device.line(line); }
        drawImpl(device);
    }

    // abstract interface for subclasses
    virtual void placeImpl() = 0;
    virtual ImVec2 inputPoint(unsigned int i) const = 0;
    virtual ImVec2 outputPoint(unsigned int i) const = 0;
    virtual void collectLines() {}; // optional
    virtual void drawImpl(Device &) const {}; // optional
};

struct IOSchema : Schema {
    IOSchema(unsigned int inputs, unsigned int outputs, float width, float height) : Schema(inputs, outputs, width, height) {
        for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
        for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
    }

    void placeImpl() override {
        const bool isLR = orientation == kLeftRight;
        const float dir = isLR ? dWire : -dWire;
        const float yMid = y + height / 2.0f;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = {isLR ? x : x + width, yMid - dWire * float(inputs - 1) / 2.0f + float(i) * dir};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = {isLR ? x + width : x, yMid - dWire * float(outputs - 1) / 2.0f + float(i) * dir};
    }

    ImVec2 inputPoint(unsigned int i) const override { return inputPoints[i]; }
    ImVec2 outputPoint(unsigned int i) const override { return outputPoints[i]; }

    std::vector<ImVec2> inputPoints;
    std::vector<ImVec2> outputPoints;
};
struct BinarySchema : Schema {
    BinarySchema(Schema *s1, Schema *s2, unsigned int inputs, unsigned int outputs, float width, float height)
        : Schema(inputs, outputs, width, height), schema1(s1), schema2(s2) {}

    ImVec2 inputPoint(unsigned int i) const override { return schema1->inputPoint(i); }
    ImVec2 outputPoint(unsigned int i) const override { return schema2->outputPoint(i); }

    void drawImpl(Device &device) const override {
        schema1->draw(device);
        schema2->draw(device);
    }

    void collectLines() override {
        schema1->collectLines();
        schema2->collectLines();
    }

    Schema *schema1;
    Schema *schema2;
};

Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link);
Schema *makeCableSchema(unsigned int n = 1);
Schema *makeInverterSchema(const string &color);
Schema *makeCutSchema();
Schema *makeEnlargedSchema(Schema *s, float width);
Schema *makeParallelSchema(Schema *s1, Schema *s2);
Schema *makeSequentialSchema(Schema *s1, Schema *s2);
Schema *makeMergeSchema(Schema *s1, Schema *s2);
Schema *makeSplitSchema(Schema *s1, Schema *s2);
Schema *makeRecSchema(Schema *s1, Schema *s2);
Schema *makeTopSchema(Schema *s, const string &link, const string &text, float margin = 10);
Schema *makeDecorateSchema(Schema *s, const string &text, float margin = 10);
Schema *makeConnectorSchema();
Schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const std::vector<int> &routes);
