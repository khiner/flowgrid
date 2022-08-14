#include "Schema.h"
#include "errors/exception.hh"

void Collector::computeVisibleTraits() {
    bool modified;
    do {
        modified = false;
        for (const auto &trait: traits) {
            if (withInput.count(trait) == 0) { // not connected to a real output
                if (outputs.count(trait.start) > 0) {
                    withInput.insert(trait); // the cable is connected to a real output
                    outputs.insert(trait.end); // end become a real output too
                    modified = true;
                }
            }
            if (withOutput.count(trait) == 0) { // not connected to a real input
                if (inputs.count(trait.end) > 0) {
                    withOutput.insert(trait); // the cable is connected to a real input
                    inputs.insert(trait.start); // start become a real input too
                    modified = true;
                }
            }
        }
    } while (modified);
}

bool Collector::isVisible(const Trait &t) const { return withInput.count(t) && withOutput.count(t); }

void Collector::draw(device &dev) {
    computeVisibleTraits();
    for (const auto &trait: traits) {
        if (isVisible(trait)) trait.draw(dev);
    }
}

bool gDrawRouteFrame = false;

/**
 * A simple rectangular box with a text and inputs and outputs.
 * The constructor is private in order to make sure `makeBlockSchema` is used instead
 */
class BlockSchema : public Schema {
protected:
    const string text;
    const string color;
    const string link;

    // fields only defined after place() is called
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    friend Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link);

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

protected:
    BlockSchema(unsigned int inputs, unsigned int outputs, double width, double height, string text, string color, string link);

    void placeInputPoints();
    void placeOutputPoints();

    void drawRectangle(device &dev);
    void drawText(device &dev);
    void drawOrientationMark(device &dev);
    void drawInputArrows(device &dev);

    void collectInputWires(Collector &c);
    void collectOutputWires(Collector &c);
};

static inline double quantize(int n) {
    static const int q = 3;
    return q * ((n + q - 1) / q); // NOLINT(bugprone-integer-division)
}

/**
 * Build a simple colored `BlockSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
 * Computes the size of the box according to the length of the text and the maximum number of ports.
 */
Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link) {
    // determine the optimal size of the box
    double minimal = 3 * dWire;
    double w = 2 * dHorz + max(minimal, dLetter * quantize((int) text.size()));
    double h = 2 * dVert + max(minimal, max(inputs, outputs) * dWire);
    return new BlockSchema(inputs, outputs, w, h, text, color, link);
}

BlockSchema::BlockSchema(unsigned int inputs, unsigned int outputs, double width, double height, string text, string color, string link)
    : Schema(inputs, outputs, width, height), text(std::move(text)), color(std::move(color)), link(std::move(link)) {
    for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
}

/**
 * Define the graphic position of the `BlockSchema`.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void BlockSchema::place(double x, double y, int orientation) {
    beginPlace(x, y, orientation);

    placeInputPoints();
    placeOutputPoints();
}

Point BlockSchema::inputPoint(unsigned int i) const { return inputPoints[i]; }
Point BlockSchema::outputPoint(unsigned int i) const { return outputPoints[i]; }

/**
 * Computes the input points according to the position and the orientation of the `BlockSchema`.
 */
void BlockSchema::placeInputPoints() {
    unsigned int N = inputs;
    if (orientation == kLeftRight) {
        double px = x;
        double py = y + (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            inputPoints[i] = {px, py + i * dWire};
        }
    } else {
        double px = x + width;
        double py = y + height - (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            inputPoints[i] = {px, py - i * dWire};
        }
    }
}

/**
 * Computes the output points according to the position and the orientation of the `BlockSchema`.
 */
void BlockSchema::placeOutputPoints() {
    unsigned int N = outputs;
    if (orientation == kLeftRight) {
        double px = x + width;
        double py = y + (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            outputPoints[i] = {px, py + i * dWire};
        }
    } else {
        double px = x;
        double py = y + height - (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            outputPoints[i] = {px, py - i * dWire};
        }
    }
}

/**
 * Draw the `BlockSchema` on the device.
 * This method can only be called after the `BlockSchema` has been placed.
 */
void BlockSchema::draw(device &dev) {
    drawRectangle(dev);
    drawText(dev);
    drawOrientationMark(dev);
    drawInputArrows(dev);
}

void BlockSchema::drawRectangle(device &dev) {
    dev.rect(x + dHorz, y + dVert, width - 2 * dHorz, height - 2 * dVert, color.c_str(), link.c_str());
}

void BlockSchema::drawText(device &dev) {
    dev.text(x + width / 2, y + height / 2, text.c_str(), link.c_str());
}

/**
 * Draw a small point that indicates the first input (like an integrated circuits).
 */
void BlockSchema::drawOrientationMark(device &dev) {
    const bool isHorz = orientation == kLeftRight;
    dev.markSens(
        x + (isHorz ? dHorz : (width - dHorz)),
        y + (isHorz ? dVert : (height - dVert)),
        orientation
    );
}

/**
 * Draw horizontal arrows from the input points to the `BlockSchema` rectangle.
 */
void BlockSchema::drawInputArrows(device &dev) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoints[i];
        dev.fleche(p.x + dx, p.y, 0, orientation);
    }
}

/**
 * Draw horizontal arrows from the input points to the `BlockSchema` rectangle.
 */
void BlockSchema::collectTraits(Collector &c) {
    collectInputWires(c);
    collectOutputWires(c);
}

/**
 * Draw horizontal arrows from the input points to the `BlockSchema` rectangle.
 */
void BlockSchema::collectInputWires(Collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoints[i];
        c.addTrait({p, {p.x + dx, p.y}});  // in->out direction
        c.addInput({p.x + dx, p.y});
    }
}

/**
 * Draw horizontal line from the `BlockSchema` rectangle to the output points
 */
void BlockSchema::collectOutputWires(Collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
    for (unsigned int i = 0; i < outputs; i++) {
        auto p = outputPoints[i];
        c.addTrait({{p.x - dx, p.y}, p});  // in->out direction
        c.addOutput({p.x - dx, p.y});
    }
}

/**
 * Simple cables (identity box) in parallel.
 * The width of a cable is null.
 * Therefore input and output connection points are the same.
 * The constructor is private to enforce the use of `makeCableSchema`.
 */
class CableSchema : public Schema {
    vector<Point> points;

public:
    friend Schema *makeCableSchema(unsigned int n);

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    CableSchema(unsigned int n);
};

/**
 * Build n cables in parallel.
 */
Schema *makeCableSchema(unsigned int n) { return new CableSchema(n); }

CableSchema::CableSchema(unsigned int n) : Schema(n, n, 0, n * dWire) {
    for (unsigned int i = 0; i < n; i++) points.emplace_back(0, 0);
}

/**
 * Place the communication points vertically spaced by `dWire`.
 */
void CableSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);
    if (orientation == kLeftRight) {
        for (unsigned int i = 0; i < inputs; i++) {
            points[i] = {ox, oy + dWire / 2.0 + i * dWire};
        }
    } else {
        for (unsigned int i = 0; i < inputs; i++) {
            points[i] = {ox, oy + height - dWire / 2.0 - i * dWire};
        }
    }
}

/**
 * Nothing to draw.
 * Actual drawing will take place when the wires are enlarged.
 */
void CableSchema::draw(device &) {}

/**
 * Nothing to collect.
 * Actual collect will take place when the wires are enlarged.
 */
void CableSchema::collectTraits(Collector &) {}

/**
 * Input and output points are the same if the width is 0.
 */
Point CableSchema::inputPoint(unsigned int i) const { return points[i]; }

/**
 * Input and output points are the same if the width is 0.
 */
Point CableSchema::outputPoint(unsigned int i) const { return points[i]; }

/**
 * An inverter is a special symbol corresponding to '*(-1)' to create more compact diagrams.
 */
class InverterSchema : public BlockSchema {
public:
    friend Schema *makeInverterSchema(const string &color);
    void draw(device &dev) override;

private:
    InverterSchema(const string &color);
};

/**
 * Build n cables in parallel.
 */
Schema *makeInverterSchema(const string &color) { return new InverterSchema(color); }

InverterSchema::InverterSchema(const string &color) : BlockSchema(1, 1, 2.5 * dWire, dWire, "-1", color, "") {}

/**
 * Nothing to draw.
 * Actual drawing will take place when the wires are enlarged.
 */
void InverterSchema::draw(device &dev) {
    dev.triangle(x + dHorz, y + 0.5, width - 2 * dHorz, height - 1, color.c_str(), link.c_str(), orientation == kLeftRight);
}

/**
 * Terminate a cable (cut box).
 */
class CutSchema : public Schema {
    Point point;

public:
    friend Schema *makeCutSchema();
    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    CutSchema();
};

Schema *makeCutSchema() { return new CutSchema(); }

/**
 * A Cut is represented by a small black dot.
 * It has 1 input and no outputs.
 * It has a 0 width and a 1 wire height.
 * The constructor is private in order to enforce the usage of `makeCutSchema`.
 */
CutSchema::CutSchema() : Schema(1, 0, 0, dWire / 100.0), point(0, 0) {}

/**
 * The input point is placed in the middle.
 */
void CutSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);
    point = {ox, oy + height * 0.5};  //, -1);
}

/**
 * A cut is represented by a small black dot.
 */
void CutSchema::draw(device &) {
    // dev.rond(point.x, point.y, dWire/8.0);
}

void CutSchema::collectTraits(Collector &) {}

/**
 * By definition, a Cut has only one input point.
 */
Point CutSchema::inputPoint(unsigned int) const { return point; }

/**
 * By definition, a Cut has no output point.
 */
Point CutSchema::outputPoint(unsigned int) const {
    faustassert(false);
    return {-1, -1};
}

class EnlargedSchema : public Schema {
    Schema *schema;
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    EnlargedSchema(Schema *s, double width);

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;
};

/**
 * Returns an enlarged schema, but only if really needed.
 * That is, if the required width is greater that the schema width.
 */
Schema *makeEnlargedSchema(Schema *s, double width) {
    return width > s->width ? new EnlargedSchema(s, width) : s;
}

/**
 * Put additional space left and right of a schema so that the result has a certain width.
 * The wires are prolonged accordingly.
 */
EnlargedSchema::EnlargedSchema(Schema *s, double width)
    : Schema(s->inputs, s->outputs, width, s->height), schema(s) {
    for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
}

/**
 * Define the graphic position of the schema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void EnlargedSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dx = (width - schema->width) / 2;
    schema->place(ox + dx, oy, orientation);

    if (orientation == kRightLeft) dx = -dx;

    for (unsigned int i = 0; i < inputs; i++) {
        auto p = schema->inputPoint(i);
        inputPoints[i] = {p.x - dx, p.y};
    }
    for (unsigned int i = 0; i < outputs; i++) {
        auto p = schema->outputPoint(i);
        outputPoints[i] = {p.x + dx, p.y};
    }
}

Point EnlargedSchema::inputPoint(unsigned int i) const { return inputPoints[i]; }
Point EnlargedSchema::outputPoint(unsigned int i) const { return outputPoints[i]; }

/**
 * Draw the enlarged schema.
 * This method can only be called after the block have been placed.
 */
void EnlargedSchema::draw(device &dev) {
    schema->draw(dev);
}

/**
 * Draw the enlarged schema.
 * This method can only be called after the block have been placed.
 */
void EnlargedSchema::collectTraits(Collector &c) {
    schema->collectTraits(c);

    // draw enlarge input wires
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoint(i);
        auto q = schema->inputPoint(i);
        c.addTrait({p, q});  // in->out direction
    }
    // draw enlarge output wires
    for (unsigned int i = 0; i < outputs; i++) {
        auto q = schema->outputPoint(i);
        auto p = outputPoint(i);
        c.addTrait({q, p});  // in->out direction
    }
}

/**
 * Place two schemi in parallel.
 */
class ParSchema : public Schema {
    Schema *schema1;
    Schema *schema2;
    unsigned int inputFrontier;
    unsigned int outputFrontier;

public:
    ParSchema(Schema *s1, Schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;
};

Schema *makeParSchema(Schema *s1, Schema *s2) {
    // make sure s1 and s2 have same width
    return new ParSchema(makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width));
}

ParSchema::ParSchema(Schema *s1, Schema *s2)
    : Schema(s1->inputs + s2->inputs, s1->outputs + s2->outputs, s1->width, s1->height + s2->height),
      schema1(s1), schema2(s2), inputFrontier(s1->inputs), outputFrontier(s1->outputs) {
    faustassert(s1->width == s2->width);
}

void ParSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    if (orientation == kLeftRight) {
        schema1->place(ox, oy, orientation);
        schema2->place(ox, oy + schema1->height, orientation);
    } else {
        schema2->place(ox, oy, orientation);
        schema1->place(ox, oy + schema2->height, orientation);
    }
}

Point ParSchema::inputPoint(unsigned int i) const {
    return i < inputFrontier ? schema1->inputPoint(i) : schema2->inputPoint(i - inputFrontier);
}

Point ParSchema::outputPoint(unsigned int i) const {
    return i < outputFrontier ? schema1->outputPoint(i) : schema2->outputPoint(i - outputFrontier);
}

void ParSchema::draw(device &dev) {
    schema1->draw(dev);
    schema2->draw(dev);
}

void ParSchema::collectTraits(Collector &c) {
    schema1->collectTraits(c);
    schema2->collectTraits(c);
}

class SeqSchema : public Schema {
    Schema *schema1;
    Schema *schema2;
    double horzGap;

public:
    friend Schema *makeSeqSchema(Schema *s1, Schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    SeqSchema(Schema *s1, Schema *s2, double hgap);
    void collectInternalWires(Collector &c);
};

enum { kHorDir, kUpDir, kDownDir };  ///< directions of connections

/**
 * Compute the direction of a connection. Note that
 * Y axis goes from top to bottom
 */
static int direction(const Point &a, const Point &b) {
    if (a.y > b.y) return kUpDir;    // upward connections
    if (a.y < b.y) return kDownDir;  // downward connection
    return kHorDir;                  // horizontal connections
}
/**
 * Compute the horizontal gap needed to draw the internal wires.
 * It depends on the largest group of connections that go in the same direction.
 */
static double computeHorzGap(Schema *a, Schema *b) {
    faustassert(a->outputs == b->inputs);

    if (a->outputs == 0) return 0;

    // store the size of the largest group for each direction
    int MaxGroupSize[3];
    for (int &i: MaxGroupSize) i = 0;

    // place a and b to have valid connection points
    double ya = max(0.0, 0.5 * (b->height - a->height));
    double yb = max(0.0, 0.5 * (a->height - b->height));
    a->place(0, ya, kLeftRight);
    b->place(0, yb, kLeftRight);

    // init current group direction and size
    int gdir = direction(a->outputPoint(0), b->inputPoint(0));
    int gsize = 1;

    // analyze direction of remaining points
    for (unsigned int i = 1; i < a->outputs; i++) {
        int d = direction(a->outputPoint(i), b->inputPoint(i));
        if (d == gdir) {
            gsize++;
        } else {
            if (gsize > MaxGroupSize[gdir]) MaxGroupSize[gdir] = gsize;
            gsize = 1;
            gdir = d;
        }
    }

    // update for last group
    if (gsize > MaxGroupSize[gdir]) MaxGroupSize[gdir] = gsize;

    // the gap required for the connections
    return dWire * max(MaxGroupSize[kUpDir], MaxGroupSize[kDownDir]);
}


/**
 * Make a sequential schema.
 * May add cables to ensure the internal connections are between the same number of outputs and inputs.
 * Compute an horizontal gap based on the number of upward and downward connections.
 */
Schema *makeSeqSchema(Schema *s1, Schema *s2) {
    unsigned int o = s1->outputs;
    unsigned int i = s2->inputs;

    Schema *a = (o < i) ? makeParSchema(s1, makeCableSchema(i - o)) : s1;
    Schema *b = (o > i) ? makeParSchema(s2, makeCableSchema(o - i)) : s2;

    return new SeqSchema(a, b, computeHorzGap(a, b));
}

/**
 * Constructor for a sequential schema (s1:s2).
 * The components s1 and s2 are supposed to be "compatible" (s1 : n->m and s2 : m->q).
 */
SeqSchema::SeqSchema(Schema *s1, Schema *s2, double hgap)
    : Schema(s1->inputs, s2->outputs, s1->width + hgap + s2->width, max(s1->height, s2->height)), schema1(s1), schema2(s2), horzGap(hgap) {
    faustassert(s1->outputs == s2->inputs);
}

/**
 * Place the two components horizontally with enough space for the connections.
 */
void SeqSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double y1 = max(0.0, 0.5 * (schema2->height - schema1->height));
    double y2 = max(0.0, 0.5 * (schema1->height - schema2->height));
    if (orientation == kLeftRight) {
        schema1->place(ox, oy + y1, orientation);
        schema2->place(ox + schema1->width + horzGap, oy + y2, orientation);
    } else {
        schema2->place(ox, oy + y2, orientation);
        schema1->place(ox + schema2->width + horzGap, oy + y1, orientation);
    }
}

/**
 * The input points are the input points of the first component.
 */
Point SeqSchema::inputPoint(unsigned int i) const { return schema1->inputPoint(i); }

/**
 * The output points are the output points of the second component.
 */
Point SeqSchema::outputPoint(unsigned int i) const { return schema2->outputPoint(i); }

/**
 * Draw the two components as well as the internal wires
 */
void SeqSchema::draw(device &dev) {
    faustassert(schema1->outputs == schema2->inputs);

    schema1->draw(dev);
    schema2->draw(dev);
}

/**
 * Draw the two components as well as the internal wires
 */
void SeqSchema::collectTraits(Collector &c) {
    faustassert(schema1->outputs == schema2->inputs);

    schema1->collectTraits(c);
    schema2->collectTraits(c);
    collectInternalWires(c);
}

/**
 * Draw the internal wires aligning the vertical segments in a symmetric way when possible.
 */
void SeqSchema::collectInternalWires(Collector &c) {
    const unsigned int N = schema1->outputs;
    faustassert(N == schema2->inputs);

    double dx = 0, mx = 0;
    int dir = -1;
    for (unsigned int i = 0; i < N; i++) {
        const auto src = schema1->outputPoint(i);
        const auto dst = schema2->inputPoint(i);
        const int d = direction(src, dst);
        if (d == dir) {
            mx += dx; // move in same direction
        } else {
            mx = orientation == kLeftRight ? (d == kDownDir ? horzGap : 0) : (d == kUpDir ? -horzGap : 0);
            dx = d == kUpDir ? dWire : d == kDownDir ? -dWire : 0;
            dir = d;
        }
        // todo add a toggle to always draw the straight cable - I tried this and it can look better imo (diagonal lines instead of manhatten)
        if (src.y == dst.y) {
            // Draw a straight, potentially diagonal cable.
            c.addTrait({src, dst});
        } else {
            // Draw a zigzag cable by traversing half the distance between, taking a sharp turn, then turning back and finishing.
            c.addTrait({src, {src.x + mx, src.y}});
            c.addTrait({{src.x + mx, src.y}, {src.x + mx, dst.y}});
            c.addTrait({{src.x + mx, dst.y}, dst});
        }
    }
}

/**
 * Place and connect two diagrams in merge composition.
 */
class MergeSchema : public Schema {
    Schema *schema1;
    Schema *schema2;
    double horzGap;

public:
    friend Schema *makeMergeSchema(Schema *s1, Schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    MergeSchema(Schema *s1, Schema *s2, double hgap);
};

/**
 * Cables are enlarged to `dWire`.
 * The horizontal gap between the two subschema is such that the connections are not too sloppy.
 */
Schema *makeMergeSchema(Schema *s1, Schema *s2) {
    // avoid ugly diagram by ensuring at least dWire width
    Schema *a = makeEnlargedSchema(s1, dWire);
    Schema *b = makeEnlargedSchema(s2, dWire);
    double hgap = (a->height + b->height) / 4;
    return new MergeSchema(a, b, hgap);
}

/**
 * Constructor for a merge schema s1 :> s2 where the outputs of s1 are merged to the inputs of s2.
 * The constructor is private in order to enforce the usage of `makeMergeSchema`.
 */
MergeSchema::MergeSchema(Schema *s1, Schema *s2, double hgap)
    : Schema(s1->inputs, s2->outputs, s1->width + s2->width + hgap, max(s1->height, s2->height)), schema1(s1), schema2(s2), horzGap(hgap) {}

/**
 * Place the two subschema horizontaly, centered, with enough gap for the connections.
 */
void MergeSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dy1 = max(0.0, schema2->height - schema1->height) / 2.0;
    double dy2 = max(0.0, schema1->height - schema2->height) / 2.0;
    if (orientation == kLeftRight) {
        schema1->place(ox, oy + dy1, orientation);
        schema2->place(ox + schema1->width + horzGap, oy + dy2, orientation);
    } else {
        schema2->place(ox, oy + dy2, orientation);
        schema1->place(ox + schema2->width + horzGap, oy + dy1, orientation);
    }
}

/**
 * The inputs of s1 :> s2 are the inputs of s1.
 */
Point MergeSchema::inputPoint(unsigned int i) const { return schema1->inputPoint(i); }

/**
 * The outputs of s1 :> s2 are the outputs of s2.
 */
Point MergeSchema::outputPoint(unsigned int i) const { return schema2->outputPoint(i); }

/**
 * Draw the two subschema and the connections between them.
 */
void MergeSchema::draw(device &dev) {
    schema1->draw(dev);
    schema2->draw(dev);
}

/**
 * Draw the two subschema and the connections between them.
 */
void MergeSchema::collectTraits(Collector &c) {
    schema1->collectTraits(c);
    schema2->collectTraits(c);

    unsigned int r = schema2->inputs;
    for (unsigned int i = 0; i < schema1->outputs; i++) {
        auto p = schema1->outputPoint(i);
        auto q = schema2->inputPoint(i % r);
        c.addTrait({p, q});
    }
}

/**
 * Place and connect two diagrams in split composition.
 */
class SplitSchema : public Schema {
    Schema *schema1;
    Schema *schema2;
    double horzGap;

public:
    friend Schema *makeSplitSchema(Schema *s1, Schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    SplitSchema(Schema *s1, Schema *s2, double hgap);
};

/**
 * Cables are enlarged to `dWire`.
 * The horizontal gap between the two subschema is such that the connections are not too sloppy.
 */
Schema *makeSplitSchema(Schema *s1, Schema *s2) {
    // make sure a and b are at least dWire large
    Schema *a = makeEnlargedSchema(s1, dWire);
    Schema *b = makeEnlargedSchema(s2, dWire);

    // horizontal gap to avoid too sloppy connections
    double hgap = (a->height + b->height) / 4;
    return new SplitSchema(a, b, hgap);
}

/**
 * Constructor for a split schema s1 <: s2, where the outputs of s1 are distributed to the inputs of s2.
 * The constructor is private in order to enforce the usage of `makeSplitSchema`.
 */
SplitSchema::SplitSchema(Schema *s1, Schema *s2, double hgap)
    : Schema(s1->inputs, s2->outputs, s1->width + s2->width + hgap, max(s1->height, s2->height)),
      schema1(s1), schema2(s2), horzGap(hgap) {}

/**
 * Places the two subschema horizontaly, centered, with enough gap for
 * the connections
 */
void SplitSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dy1 = max(0.0, schema2->height - schema1->height) / 2.0;
    double dy2 = max(0.0, schema1->height - schema2->height) / 2.0;
    if (orientation == kLeftRight) {
        schema1->place(ox, oy + dy1, orientation);
        schema2->place(ox + schema1->width + horzGap, oy + dy2, orientation);
    } else {
        schema2->place(ox, oy + dy2, orientation);
        schema1->place(ox + schema2->width + horzGap, oy + dy1, orientation);
    }
}

/**
 * The inputs of s1 <: s2 are the inputs of s1.
 */
Point SplitSchema::inputPoint(unsigned int i) const { return schema1->inputPoint(i); }

/**
 * The outputs of s1 <: s2 are the outputs of s2.
 */
Point SplitSchema::outputPoint(unsigned int i) const { return schema2->outputPoint(i); }

/**
 * Draw the two sub schema and the connections between them.
 */
void SplitSchema::draw(device &dev) {
    schema1->draw(dev);
    schema2->draw(dev);
}

/**
 * Draw the two subschema and the connections between them.
 */
void SplitSchema::collectTraits(Collector &c) {
    schema1->collectTraits(c);
    schema2->collectTraits(c);

    unsigned int r = schema1->outputs;
    for (unsigned int i = 0; i < schema2->inputs; i++) {
        auto p = schema1->outputPoint(i % r);
        auto q = schema2->inputPoint(i);
        c.addTrait({p, q});
    }
}


/**
 * Place and connect two diagrams in recursive composition
 */
class RecSchema : public Schema {
    Schema *schema1;
    Schema *schema2;
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    friend Schema *makeRecSchema(Schema *s1, Schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    RecSchema(Schema *s1, Schema *s2, double width);

    void collectFeedback(Collector &c, const Point &src, const Point &dst, double dx, const Point &out);
    void collectFeedfront(Collector &c, const Point &src, const Point &dst, double dx);
};

/**
 * Creates a new recursive schema (s1 ~ s2).
 * The smallest component is enlarged to the width of the other.
 * The left and right horizontal margins are computed according to the number of internal connections.
 */
Schema *makeRecSchema(Schema *s1, Schema *s2) {
    Schema *a = makeEnlargedSchema(s1, s2->width);
    Schema *b = makeEnlargedSchema(s2, s1->width);
    double m = dWire * max(b->inputs, b->outputs);
    double w = a->width + 2 * m;

    return new RecSchema(a, b, w);
}

/**
 * Constructor of a recursive schema (s1 ~ s2).
 * The two components are supposed to have the same width.
 */
RecSchema::RecSchema(Schema *s1, Schema *s2, double width)
    : Schema(s1->inputs - s2->outputs, s1->outputs, width, s1->height + s2->height), schema1(s1), schema2(s2) {
    // this version only accepts legal expressions of same width
    faustassert(s1->inputs >= s2->outputs);
    faustassert(s1->outputs >= s2->inputs);
    faustassert(s1->width >= s2->width);

    for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
}

/**
 * The two subschema are placed centered vertically, s2 on top of s1.
 * The input and output points are computed.
 */
void RecSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dx1 = (width - schema1->width) / 2;
    double dx2 = (width - schema2->width) / 2;
    if (orientation == kLeftRight) {
        schema2->place(ox + dx2, oy, kRightLeft);
        schema1->place(ox + dx1, oy + schema2->height, kLeftRight);
    } else {
        schema1->place(ox + dx1, oy, kRightLeft);
        schema2->place(ox + dx2, oy + schema1->height, kLeftRight);
    }

    // adjust delta space to orientation
    if (orientation == kRightLeft) dx1 = -dx1;

    for (unsigned int i = 0; i < inputs; i++) {
        auto p = schema1->inputPoint(i + schema2->outputs);
        inputPoints[i] = {p.x - dx1, p.y};
    }
    for (unsigned int i = 0; i < outputs; i++) {
        auto p = schema1->outputPoint(i);
        outputPoints[i] = {p.x + dx1, p.y};
    }
}

/**
 * The input points s1 ~ s2
 */
Point RecSchema::inputPoint(unsigned int i) const { return inputPoints[i]; }

/**
 * The output points s1 ~ s2
 */
Point RecSchema::outputPoint(unsigned int i) const { return outputPoints[i]; }

/**
 * Draw the delay sign of a feedback connection
 */
static void drawDelaySign(device &dev, double x, double y, double size) {
    dev.trait(x - size / 2, y, x - size / 2, y - size);
    dev.trait(x - size / 2, y - size, x + size / 2, y - size);
    dev.trait(x + size / 2, y - size, x + size / 2, y);
}

/**
 * Draw the two subschema s1 and s2 as well as the implicit feedback
 * delays between s1 and s2.
 */
void RecSchema::draw(device &dev) {
    schema1->draw(dev);
    schema2->draw(dev);

    // draw the implicit feedback delay to each schema2 input
    double dw = (orientation == kLeftRight) ? dWire : -dWire;
    for (unsigned int i = 0; i < schema2->inputs; i++) {
        const auto &p = schema1->outputPoint(i);
        drawDelaySign(dev, p.x + i * dw, p.y, dw / 2);
    }
}

/**
 * Draw the two subschema s1 and s2 as well as the feedback
 * connections between s1 and s2, and the feedfrom connections
 * beetween s2 and s1.
 */
void RecSchema::collectTraits(Collector &c) {
    schema1->collectTraits(c);
    schema2->collectTraits(c);

    // draw the feedback connections to each schema2 input
    for (unsigned int i = 0; i < schema2->inputs; i++) {
        collectFeedback(c, schema1->outputPoint(i), schema2->inputPoint(i), i * dWire, outputPoint(i));
    }
    // draw the non-recursive output lines
    for (unsigned int i = schema2->inputs; i < outputs; i++) {
        auto p = schema1->outputPoint(i);
        auto q = outputPoint(i);
        c.addTrait({p, q});  // in->out order
    }

    // draw the input lines
    unsigned int skip = schema2->outputs;
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoint(i);
        auto q = schema1->inputPoint(i + skip);
        c.addTrait({p, q});  // in->out order
    }

    // draw the feedfront connections from each schema2 output
    for (unsigned int i = 0; i < schema2->outputs; i++) {
        collectFeedfront(c, schema2->outputPoint(i), schema1->inputPoint(i), i * dWire);
    }
}

/**
 * Draw a feedback connection between two points with an horizontal displacement `dx`.
 */
void RecSchema::collectFeedback(Collector &c, const Point &src, const Point &dst, double dx, const Point &out) {
    double ox = src.x + ((orientation == kLeftRight) ? dx : -dx);
    double ct = (orientation == kLeftRight) ? dWire / 2 : -dWire / 2;

    Point up(ox, src.y - ct);
    Point br(ox + ct / 2.0, src.y);

    c.addOutput(up);
    c.addOutput(br);
    c.addInput(br);

    c.addTrait({up, {ox, dst.y}});
    c.addTrait({{ox, dst.y}, dst});
    c.addTrait({src, br});
    c.addTrait({br, out});
}

/**
 * Draw a feedfrom connection between two points with an horizontal displacement `dx`.
 */
void RecSchema::collectFeedfront(Collector &c, const Point &src, const Point &dst, double dx) {
    double ox = src.x + ((orientation == kLeftRight) ? -dx : dx);

    c.addTrait({{src.x, src.y}, {ox, src.y}});
    c.addTrait({{ox, src.y}, {ox, dst.y}});
    c.addTrait({{ox, dst.y}, {dst.x, dst.y}});
}

/**
 * A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 * Arrows are added to all the outputs.
 */

class TopSchema : public Schema {
    Schema *schema;
    double fMargin;
    string text;
    string link;
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    friend Schema *makeTopSchema(Schema *s1, double margin, const string &text, const string &link);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    TopSchema(Schema *s1, double margin, string text, string link);
};

Schema *makeTopSchema(Schema *s, double margin, const string &text, const string &link) {
    return new TopSchema(makeDecorateSchema(s, margin / 2, text), margin / 2, "", link);
}

/**
 * A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 * Arrows are added to the outputs.
 * The constructor is made private to enforce the usage of `makeTopSchema`.
 */
TopSchema::TopSchema(Schema *s, double margin, string text, string link)
    : Schema(0, 0, s->width + 2 * margin, s->height + 2 * margin), schema(s), fMargin(margin), text(std::move(text)), link(std::move(link)) {}

/**
 * Define the graphic position of the schema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void TopSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    schema->place(ox + fMargin, oy + fMargin, orientation);
}

/**
 * Top schema has no input
 */
Point TopSchema::inputPoint(unsigned int) const { throw faustexception("ERROR : TopSchema::inputPoint\n"); }

/**
 * Top schema has no output
 */
Point TopSchema::outputPoint(unsigned int) const { throw faustexception("ERROR : TopSchema::outputPoint\n"); }

/**
 * Draw the enlarged schema. This methos can only
 * be called after the block have been placed
 */
void TopSchema::draw(device &dev) {
    // draw a background white rectangle
    dev.rect(x, y, width - 1, height - 1, "#ffffff", link.c_str());
    // draw the label
    dev.label(x + fMargin, y + fMargin / 2, text.c_str());

    schema->draw(dev);

    // draw arrows at output points of schema
    for (unsigned int i = 0; i < schema->outputs; i++) {
        auto p = schema->outputPoint(i);
        dev.fleche(p.x, p.y, 0, orientation);
    }
}

/**
 * Draw the enlarged schema. This method can only
 * be called after the block have been placed
 */
void TopSchema::collectTraits(Collector &c) {
    schema->collectTraits(c);

    for (unsigned int i = 0; i < schema->inputs; i++) {
        auto p = schema->inputPoint(i);
        c.addOutput(p);
    }
    for (unsigned int i = 0; i < schema->outputs; i++) {
        auto p = schema->outputPoint(i);
        c.addInput(p);
    }
}

/**
 * A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 */
class DecorateSchema : public Schema {
    Schema *schema;
    double fMargin;
    string text;
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    friend Schema *makeDecorateSchema(Schema *s1, double margin, const string &text);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

private:
    DecorateSchema(Schema *s1, double margin, string text);
};

Schema *makeDecorateSchema(Schema *s, double margin, const string &text) { return new DecorateSchema(s, margin, text); }

/**
 * A DecorateSchema is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 * The constructor is made private to enforce the usage of `makeDecorateSchema`
 */
DecorateSchema::DecorateSchema(Schema *s, double margin, string text)
    : Schema(s->inputs, s->outputs, s->width + 2 * margin, s->height + 2 * margin), schema(s), fMargin(margin), text(std::move(text)) {
    for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
}

/**
 * Define the graphic position of the schema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void DecorateSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    schema->place(ox + fMargin, oy + fMargin, orientation);

    double m = fMargin;
    if (orientation == kRightLeft) m = -m;

    for (unsigned int i = 0; i < inputs; i++) {
        auto p = schema->inputPoint(i);
        inputPoints[i] = {p.x - m, p.y};
    }
    for (unsigned int i = 0; i < outputs; i++) {
        auto p = schema->outputPoint(i);
        outputPoints[i] = {p.x + m, p.y};
    }
}

Point DecorateSchema::inputPoint(unsigned int i) const { return inputPoints[i]; }
Point DecorateSchema::outputPoint(unsigned int i) const { return outputPoints[i]; }

void DecorateSchema::draw(device &dev) {
    schema->draw(dev);

    // define the coordinates of the frame
    double tw = (2 + text.size()) * dLetter * 0.75;
    double x0 = x + fMargin / 2;             // left
    double y0 = y + fMargin / 2;             // top
    double x1 = x + width - fMargin / 2;   // right
    double y1 = y + height - fMargin / 2;  // bottom
    double tl = x + fMargin;     // left of text zone
    double tr = min(tl + tw, x1);  // right of text zone

    // draw the surrounding frame
    dev.dasharray(x0, y0, x0, y1);  // left line
    dev.dasharray(x0, y1, x1, y1);  // bottom line
    dev.dasharray(x1, y1, x1, y0);  // right line
    dev.dasharray(x0, y0, tl, y0);  // top segment before text
    dev.dasharray(tr, y0, x1, y0);  // top segment after text

    // draw the label
    dev.label(tl, y0, text.c_str());  //
}

void DecorateSchema::collectTraits(Collector &c) {
    schema->collectTraits(c);

    for (unsigned int i = 0; i < inputs; i++) c.addTrait({inputPoint(i), schema->inputPoint(i)});
    for (unsigned int i = 0; i < outputs; i++) c.addTrait({schema->outputPoint(i), outputPoint(i)});
}

/**
 * A simple rectangular box with a text and inputs and outputs.
 * The constructor is private in order to make sure `makeConnectorSchema` is used instead.
 */
class ConnectorSchema : public Schema {
protected:
    // fields only defined after place() is called
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    friend Schema *makeConnectorSchema();

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

protected:
    ConnectorSchema();

    void placeInputPoints();
    void placeOutputPoints();
    void collectInputWires(Collector &c);
    void collectOutputWires(Collector &c);
};

/**
 * Connectors are used to ensure unused inputs and outputs are drawn.
 */
Schema *makeConnectorSchema() { return new ConnectorSchema(); }

/**
 * A connector is an invisible square for `dWire` size with 1 input and 1 output.
 */
ConnectorSchema::ConnectorSchema() : Schema(1, 1, dWire, dWire) {
    inputPoints.emplace_back(0, 0);
    outputPoints.emplace_back(0, 0);
}

/**
 * Define the graphic position of the ConnectorSchema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void ConnectorSchema::place(double x, double y, int orientation) {
    beginPlace(x, y, orientation);

    placeInputPoints();
    placeOutputPoints();
}

Point ConnectorSchema::inputPoint(unsigned int i) const { return inputPoints[i]; }
Point ConnectorSchema::outputPoint(unsigned int i) const { return outputPoints[i]; }

/**
 * Computes the input points according to the position and the orientation of the ConnectorSchema
 */
void ConnectorSchema::placeInputPoints() {
    unsigned int N = inputs;
    if (orientation == kLeftRight) {
        double px = x;
        double py = y + (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            inputPoints[i] = {px, py + i * dWire};
        }
    } else {
        double px = x + width;
        double py = y + height - (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            inputPoints[i] = {px, py - i * dWire};
        }
    }
}

/**
 * Computes the output points according to the position and the orientation of the ConnectorSchema.
 */
void ConnectorSchema::placeOutputPoints() {
    unsigned int N = outputs;
    if (orientation == kLeftRight) {
        double px = x + width;
        double py = y + (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            outputPoints[i] = {px, py + i * dWire};
        }
    } else {
        double px = x;
        double py = y + height - (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            outputPoints[i] = {px, py - i * dWire};
        }
    }
}

/**
 * Draw the ConnectorSchema on the device.
 * This method can only be called after the `ConnectorSchema` has been placed.
 */
void ConnectorSchema::draw(device &) {}

/**
 * Draw horizontal arrows from the input points to the ConnectorSchema rectangle.
 */
void ConnectorSchema::collectTraits(Collector &c) {
    collectInputWires(c);
    collectOutputWires(c);
}

/**
 * Draw horizontal arrows from the input points to the
 * ConnectorSchema rectangle
 */
void ConnectorSchema::collectInputWires(Collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoints[i];
        c.addTrait({p, {p.x + dx, p.y}});  // in->out direction
        c.addInput({p.x + dx, p.y});
    }
}

/**
 * Draw horizontal line from the ConnectorSchema rectangle to the output points
 */
void ConnectorSchema::collectOutputWires(Collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
    for (unsigned int i = 0; i < outputs; i++) {
        auto p = outputPoints[i];
        c.addTrait({{p.x - dx, p.y}, p});  // in->out direction
        c.addOutput({p.x - dx, p.y});
    }
}

/**
 * A simple rectangular box with a text and inputs and outputs.
 * The constructor is private in order to make sure `makeBlockSchema` is used instead.
 */
class RouteSchema : public Schema {
protected:
    const string text;
    const string color;
    const string link;
    const std::vector<int> routes;  ///< route description: s1,d2,s2,d2,...

    // fields only defined after place() is called
    vector<Point> inputPoints;
    vector<Point> outputPoints;

public:
    friend Schema *makeRouteSchema(unsigned int n, unsigned int m, const std::vector<int> &routes);
    // friend schema* makeRoutingSchema(unsigned int inputs, unsigned int outputs, const vector<int>& route);
    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    Point inputPoint(unsigned int i) const override;
    Point outputPoint(unsigned int i) const override;
    void collectTraits(Collector &c) override;

protected:
    RouteSchema(unsigned int inputs, unsigned int outputs, double width, double height, const std::vector<int> &routes);

    void placeInputPoints();
    void placeOutputPoints();

    void drawRectangle(device &dev);
    void drawText(device &dev);
    void drawOrientationMark(device &dev);
    void drawInputArrows(device &dev);

    void collectInputWires(Collector &c);
    void collectOutputWires(Collector &c);
};

/**
 * Build n x m cable routing
 */
Schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const std::vector<int> &routes) {
    // determine the optimal size of the box
    double minimal = 3 * dWire;
    double h = 2 * dVert + max(minimal, max(inputs, outputs) * dWire);
    double w = 2 * dHorz + max(minimal, h * 0.75);

    return new RouteSchema(inputs, outputs, w, h, routes);
}

/**
 * Build a simple colored `RouteSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
 * The length of the text as well as the number of inputs and outputs are used to compute the size of the `RouteSchema`
 */
RouteSchema::RouteSchema(unsigned int inputs, unsigned int outputs, double width, double height, const std::vector<int> &routes)
    : Schema(inputs, outputs, width, height), text(""), color("#EEEEAA"), link(""), routes(routes) {
    for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(Point(0, 0));
    for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
}

/**
 * Define the graphic position of the `RouteSchema`.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void RouteSchema::place(double x, double y, int orientation) {
    beginPlace(x, y, orientation);

    placeInputPoints();
    placeOutputPoints();
}

Point RouteSchema::inputPoint(unsigned int i) const { return inputPoints[i]; }
Point RouteSchema::outputPoint(unsigned int i) const { return outputPoints[i]; }

/**
 * Computes the input points according to the position and the orientation of the `RouteSchema`.
 */
void RouteSchema::placeInputPoints() {
    unsigned int N = inputs;
    if (orientation == kLeftRight) {
        double px = x;
        double py = y + (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            inputPoints[i] = {px, py + i * dWire};
        }
    } else {
        double px = x + width;
        double py = y + height - (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            inputPoints[i] = {px, py - i * dWire};
        }
    }
}

/**
 * Computes the output points according to the position and the orientation of the `RouteSchema`.
 */
void RouteSchema::placeOutputPoints() {
    unsigned int N = outputs;
    if (orientation == kLeftRight) {
        double px = x + width;
        double py = y + (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            outputPoints[i] = {px, py + i * dWire};
        }
    } else {
        double px = x;
        double py = y + height - (height - dWire * (N - 1)) / 2;
        for (unsigned int i = 0; i < N; i++) {
            outputPoints[i] = {px, py - i * dWire};
        }
    }
}

/**
 * Draw the `RouteSchema` on the device.
 * This method can only be called after the `RouteSchema` have been placed.
 */
void RouteSchema::draw(device &dev) {
    if (gDrawRouteFrame) {
        drawRectangle(dev);
        // drawText(dev);
        drawOrientationMark(dev);
        drawInputArrows(dev);
    }
}

/**
 * Draw the colored rectangle with the optional link
 */
void RouteSchema::drawRectangle(device &dev) {
    dev.rect(x + dHorz, y + dVert, width - 2 * dHorz, height - 2 * dVert, color.c_str(), link.c_str());
}

/**
 * Draw the text centered on the box
 */
void RouteSchema::drawText(device &dev) {
    dev.text(x + width / 2, y + height / 2, text.c_str(), link.c_str());
}

/**
 * Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
 */
void RouteSchema::drawOrientationMark(device &dev) {
    const bool isHorz = orientation == kLeftRight;
    dev.markSens(
        x + (isHorz ? dHorz : (width - dHorz)),
        y + (isHorz ? dVert : (height - dVert)),
        orientation
    );
}

/**
 * Draw horizontal arrows from the input points to the `RouteSchema` rectangle.
 */
void RouteSchema::drawInputArrows(device &dev) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoints[i];
        dev.fleche(p.x + dx, p.y, 0, orientation);
    }
}

/**
 * Draw horizontal arrows from the input points to the `RouteSchema` rectangle.
 */
void RouteSchema::collectTraits(Collector &c) {
    collectInputWires(c);
    collectOutputWires(c);
    // additional routing traits
    for (unsigned int i = 0; i < routes.size() - 1; i += 2) {
        int src = routes[i] - 1;
        int dst = routes[i + 1] - 1;
        auto p1 = inputPoints[src];
        auto p2 = outputPoints[dst];
        double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
        c.addTrait({{p1.x + dx, p1.y}, {p2.x - dx, p2.y}});
    }
}

/**
 * Draw horizontal arrows from the input points to the `RouteSchema` rectangle.
 */
void RouteSchema::collectInputWires(Collector &c) {
    double dx = orientation == kLeftRight ? dHorz : -dHorz;
    for (unsigned int i = 0; i < inputs; i++) {
        auto p = inputPoints[i];
        c.addTrait({p, {p.x + dx, p.y}});  // in->out direction
        c.addInput({p.x + dx, p.y});
    }
}

/**
 * Draw horizontal line from the `RouteSchema` rectangle to the output points.
 */
void RouteSchema::collectOutputWires(Collector &c) {
    double dx = orientation == kLeftRight ? dHorz : -dHorz;
    for (unsigned int i = 0; i < outputs; i++) {
        auto p = outputPoints[i];
        c.addTrait({{p.x - dx, p.y}, p});  // in->out direction
        c.addOutput({p.x - dx, p.y});
    }
}
