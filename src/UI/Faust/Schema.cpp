#include "Schema.h"
#include "errors/exception.hh"

void collector::computeVisibleTraits() {
    bool modified;

    do {
        modified = false;
        for (const auto &fTrait: traits) {
            if (withInput.count(fTrait) == 0) { // not connected to a real output
                if (outputs.count(fTrait.start) > 0) {
                    withInput.insert(fTrait); // the cable is connected to a real output
                    outputs.insert(fTrait.end); // end become a real output too
                    modified = true;
                }
            }
            if (withOutput.count(fTrait) == 0) { // not connected to a real input
                if (inputs.count(fTrait.end) > 0) {
                    withOutput.insert(fTrait); // the cable is connected to a real input
                    inputs.insert(fTrait.start); // start become a real input too
                    modified = true;
                }
            }
        }
    } while (modified);
}

bool collector::isVisible(const trait &t) { return withInput.count(t) && withOutput.count(t); }

void collector::draw(device &dev) {
    computeVisibleTraits();
    for (const auto &fTrait: traits) {
        if (isVisible(fTrait)) fTrait.draw(dev);
    }
}

bool gDrawRouteFrame = false;

/**
 * A simple rectangular box with a text and inputs and outputs.
 * The constructor is private in order to make sure `makeBlockSchema` is used instead
 */
class blockSchema : public schema {
protected:
    const string fText;
    const string fColor;
    const string fLink;

    // fields only defined after place() is called
    vector<point> fInputPoint;
    vector<point> fOutputPoint;

public:
    friend schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &name, const string &color, const string &link);

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

protected:
    blockSchema(unsigned int inputs, unsigned int outputs, double width, double height, const string &name, const string &color, const string &link);

    void placeInputPoints();
    void placeOutputPoints();

    void drawRectangle(device &dev);
    void drawText(device &dev);
    void drawOrientationMark(device &dev);
    void drawInputArrows(device &dev);

    void collectInputWires(collector &c);
    void collectOutputWires(collector &c);
};

static double quantize(int n) {
    int q = 3;
    return dLetter * (q * ((n + q - 1) / q));
}

/**
 * Build a simple colored `blockSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
 * Computes the size of the box according to the length of the text and the maximum number of ports.
 */
schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link) {
    // determine the optimal size of the box
    double minimal = 3 * dWire;
    double w = 2 * dHorz + max(minimal, quantize((int) text.size()));
    double h = 2 * dVert + max(minimal, max(inputs, outputs) * dWire);

    return new blockSchema(inputs, outputs, w, h, text, color, link);
}

blockSchema::blockSchema(unsigned int inputs, unsigned int outputs, double width, double height, const string &text, const string &color, const string &link)
    : schema(inputs, outputs, width, height), fText(text), fColor(color), fLink(link) {
    for (unsigned int i = 0; i < inputs; i++) fInputPoint.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) fOutputPoint.emplace_back(0, 0);
}

/**
 * Define the graphic position of the `blockSchema`.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void blockSchema::place(double x, double y, int orientation) {
    beginPlace(x, y, orientation);

    placeInputPoints();
    placeOutputPoints();

    endPlace();
}

point blockSchema::inputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < inputs);
    return fInputPoint[i];
}

point blockSchema::outputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < outputs);
    return fOutputPoint[i];
}

/**
 * Computes the input points according to the position and the orientation of the `blockSchema`.
 */
void blockSchema::placeInputPoints() {
    int N = inputs;

    if (orientation == kLeftRight) {
        double px = x;
        double py = y + (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fInputPoint[i] = point(px, py + i * dWire);
        }
    } else {
        double px = x + width;
        double py = y + height - (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fInputPoint[i] = point(px, py - i * dWire);
        }
    }
}

/**
 * Computes the output points according to the position and the orientation of the `blockSchema`.
 */
void blockSchema::placeOutputPoints() {
    int N = outputs;

    if (orientation == kLeftRight) {
        double px = x + width;
        double py = y + (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fOutputPoint[i] = point(px, py + i * dWire);
        }

    } else {
        double px = x;
        double py = y + height - (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fOutputPoint[i] = point(px, py - i * dWire);
        }
    }
}

/**
 * Draw the `blockSchema` on the device.
 * This method can only be called after the `blockSchema` has been placed.
 */
void blockSchema::draw(device &dev) {
    faustassert(placed());

    drawRectangle(dev);
    drawText(dev);
    drawOrientationMark(dev);
    drawInputArrows(dev);
}

/**
 * Draw the colored rectangle with the optional link.
 */
void blockSchema::drawRectangle(device &dev) {
    dev.rect(x + dHorz, y + dVert, width - 2 * dHorz, height - 2 * dVert, fColor.c_str(), fLink.c_str());
}

/**
 * Draw the text centered on the box.
 */
void blockSchema::drawText(device &dev) {
    dev.text(x + width / 2, y + height / 2, fText.c_str(), fLink.c_str());
}

/**
 * Draw the orientation mark, a small point that indicates the first input (like an integrated circuits).
 */
void blockSchema::drawOrientationMark(device &dev) {
    double px, py;

    if (orientation == kLeftRight) {
        px = x + dHorz;
        py = y + dVert;
    } else {
        px = x + width - dHorz;
        py = y + height - dVert;
    }

    dev.markSens(px, py, orientation);
}

/**
 * Draw horizontal arrows from the input points to the `blockSchema` rectangle.
 */
void blockSchema::drawInputArrows(device &dev) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fInputPoint[i];
        dev.fleche(p.x + dx, p.y, 0, orientation);
    }
}

/**
 * Draw horizontal arrows from the input points to the `blockSchema` rectangle.
 */
void blockSchema::collectTraits(collector &c) {
    collectInputWires(c);
    collectOutputWires(c);
}

/**
 * Draw horizontal arrows from the input points to the `blockSchema` rectangle.
 */
void blockSchema::collectInputWires(collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fInputPoint[i];
        c.addTrait(trait(point(p.x, p.y), point(p.x + dx, p.y)));  // in->out direction
        c.addInput(point(p.x + dx, p.y));
    }
}

/**
 * Draw horizontal line from the blockSchema rectangle to the
 * output points
 */
void blockSchema::collectOutputWires(collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < outputs; i++) {
        point p = fOutputPoint[i];
        c.addTrait(trait(point(p.x - dx, p.y), point(p.x, p.y)));  // in->out direction
        c.addOutput(point(p.x - dx, p.y));
    }
}

/**
 * Simple cables (identity box) in parallel.
 * The width of a cable is null.
 * Therefore input and output connection points are the same.
 * The constructor is private to enforce the use of `makeCableSchema`.
 */
class cableSchema : public schema {
    vector<point> fPoint;

public:
    friend schema *makeCableSchema(unsigned int n);

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    cableSchema(unsigned int n);
};

/**
 * Build n cables in parallel.
 */
schema *makeCableSchema(unsigned int n) {
    faustassert(n > 0);
    return new cableSchema(n);
}

/**
 * Build n cables in parallel.
 */
cableSchema::cableSchema(unsigned int n) : schema(n, n, 0, n * dWire) {
    for (unsigned int i = 0; i < n; i++) fPoint.emplace_back(0, 0);
}

/**
 * Place the communication points vertically spaced by `dWire`.
 */
void cableSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);
    if (orientation == kLeftRight) {
        for (unsigned int i = 0; i < inputs; i++) {
            fPoint[i] = point(ox, oy + dWire / 2.0 + i * dWire);
        }
    } else {
        for (unsigned int i = 0; i < inputs; i++) {
            fPoint[i] = point(ox, oy + height - dWire / 2.0 - i * dWire);
        }
    }
    endPlace();
}

/**
 * Nothing to draw.
 * Actual drawing will take place when the wires are enlarged.
 */
void cableSchema::draw(device &) {}

/**
 * Nothing to collect.
 * Actual collect will take place when the wires are enlarged.
 */
void cableSchema::collectTraits(collector &) {}

/**
 * Input and output points are the same if the width is 0.
 */
point cableSchema::inputPoint(unsigned int i) const {
    faustassert(i < inputs);
    return fPoint[i];
}

/**
 * Input and output points are the same if the width is 0.
 */
point cableSchema::outputPoint(unsigned int i) const {
    faustassert(i < outputs);
    return fPoint[i];
}

/**
 * An inverter is a special symbol corresponding to '*(-1)' to create more compact diagrams.
 */
class inverterSchema : public blockSchema {
public:
    friend schema *makeInverterSchema(const string &color);
    void draw(device &dev) override;

private:
    inverterSchema(const string &color);
};

/**
 * Build n cables in parallel.
 */
schema *makeInverterSchema(const string &color) { return new inverterSchema(color); }

/**
 * Build n cables in parallel.
 */
inverterSchema::inverterSchema(const string &color) : blockSchema(1, 1, 2.5 * dWire, dWire, "-1", color, "") {}

/**
 * Nothing to draw.
 * Actual drawing will take place when the wires are enlargered.
 */
void inverterSchema::draw(device &dev) {
    dev.triangle(x + dHorz, y + 0.5, width - 2 * dHorz, height - 1, fColor.c_str(), fLink.c_str(), orientation == kLeftRight);
}

/**
 * Terminate a cable (cut box).
 */
class cutSchema : public schema {
    point fPoint;

public:
    friend schema *makeCutSchema();
    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    cutSchema();
};

/**
 * Creates a new Cut schema.
 */
schema *makeCutSchema() { return new cutSchema(); }

/**
 * A Cut is represented by a small black dot.
 * It has 1 input and no outputs.
 * It has a 0 width and a 1 wire height.
 * The constructor is private in order to enforce the usage of `makeCutSchema`.
 */
cutSchema::cutSchema() : schema(1, 0, 0, dWire / 100.0), fPoint(0, 0) {}

/**
 * The input point is placed in the middle.
 */
void cutSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);
    fPoint = point(ox, oy + height * 0.5);  //, -1);
    endPlace();
}

/**
 * A cut is represented by a small black dot.
 */
void cutSchema::draw(device &) {
    // dev.rond(fPoint.x, fPoint.y, dWire/8.0);
}

void cutSchema::collectTraits(collector &) {}

/**
 * By definition, a Cut has only one input point.
 */
point cutSchema::inputPoint(unsigned int i) const {
    faustassert(i == 0);
    return fPoint;
}

/**
 * By definition, a Cut has no output point.
 */
point cutSchema::outputPoint(unsigned int) const {
    faustassert(false);
    return {-1, -1};
}

class enlargedSchema : public schema {
    schema *fSchema;
    vector<point> fInputPoint;
    vector<point> fOutputPoint;

public:
    enlargedSchema(schema *s, double width);

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;
};

/**
 * Returns an enlarged schema, but only if really needed.
 * That is, if the required width is greater that the schema width.
 */
schema *makeEnlargedSchema(schema *s, double width) {
    return width > s->width ? new enlargedSchema(s, width) : s;
}

/**
 * Put additional space left and right of a schema so that the result has a certain width.
 * The wires are prolonged accordingly.
 */
enlargedSchema::enlargedSchema(schema *s, double width)
    : schema(s->inputs, s->outputs, width, s->height), fSchema(s) {
    for (unsigned int i = 0; i < inputs; i++) fInputPoint.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) fOutputPoint.emplace_back(0, 0);
}

/**
 * Define the graphic position of the schema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void enlargedSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dx = (width - fSchema->width) / 2;
    fSchema->place(ox + dx, oy, orientation);

    if (orientation == kRightLeft) dx = -dx;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fSchema->inputPoint(i);
        fInputPoint[i] = point(p.x - dx, p.y);  //, p.z);
    }

    for (unsigned int i = 0; i < outputs; i++) {
        point p = fSchema->outputPoint(i);
        fOutputPoint[i] = point(p.x + dx, p.y);  //, p.z);
    }

    endPlace();
}

point enlargedSchema::inputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < inputs);
    return fInputPoint[i];
}

point enlargedSchema::outputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < outputs);
    return fOutputPoint[i];
}

/**
 * Draw the enlarged schema.
 * This method can only be called after the block have been placed.
 */
void enlargedSchema::draw(device &dev) {
    faustassert(placed());

    fSchema->draw(dev);
#if 0
    // draw enlarge input wires
    for (unsigned int i=0; i<fInputs; i++) {
        point p = inputPoint(i);
        point q = fSchema->inputPoint(i);
        if ( (p.z>=0) && (q.z>=0) ) dev.trait(p.x, p.y, q.x, q.y);
    }

    // draw enlarge output wires
    for (unsigned int i=0; i<fOutputs; i++) {
        point p = outputPoint(i);
        point q = fSchema->outputPoint(i);
        if ( (p.z>=0) && (q.z>=0) ) dev.trait(p.x, p.y, q.x, q.y);
    }
#endif
}

/**
 * Draw the enlarged schema.
 * This method can only be called after the block have been placed.
 */
void enlargedSchema::collectTraits(collector &c) {
    faustassert(placed());

    fSchema->collectTraits(c);

    // draw enlarge input wires
    for (unsigned int i = 0; i < inputs; i++) {
        point p = inputPoint(i);
        point q = fSchema->inputPoint(i);
        c.addTrait(trait(p, q));  // in->out direction
    }

    // draw enlarge output wires
    for (unsigned int i = 0; i < outputs; i++) {
        point q = fSchema->outputPoint(i);
        point p = outputPoint(i);
        c.addTrait(trait(q, p));  // in->out direction
    }
}

/**
 * Place two schemi in parallel.
 */
class parSchema : public schema {
    schema *fSchema1;
    schema *fSchema2;
    unsigned int fInputFrontier;
    unsigned int fOutputFrontier;

public:
    parSchema(schema *s1, schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;
};

schema *makeParSchema(schema *s1, schema *s2) {
    // make sure s1 and s2 have same width
    return new parSchema(makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width));
}

parSchema::parSchema(schema *s1, schema *s2)
    : schema(s1->inputs + s2->inputs, s1->outputs + s2->outputs, s1->width, s1->height + s2->height),
      fSchema1(s1), fSchema2(s2), fInputFrontier(s1->inputs), fOutputFrontier(s1->outputs) {
    faustassert(s1->width == s2->width);
}

void parSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    if (orientation == kLeftRight) {
        fSchema1->place(ox, oy, orientation);
        fSchema2->place(ox, oy + fSchema1->height, orientation);
    } else {
        fSchema2->place(ox, oy, orientation);
        fSchema1->place(ox, oy + fSchema2->height, orientation);
    }

    endPlace();
}

point parSchema::inputPoint(unsigned int i) const {
    return (i < fInputFrontier) ? fSchema1->inputPoint(i) : fSchema2->inputPoint(i - fInputFrontier);
}

point parSchema::outputPoint(unsigned int i) const {
    return (i < fOutputFrontier) ? fSchema1->outputPoint(i) : fSchema2->outputPoint(i - fOutputFrontier);
}

void parSchema::draw(device &dev) {
    fSchema1->draw(dev);
    fSchema2->draw(dev);
}

void parSchema::collectTraits(collector &c) {
    fSchema1->collectTraits(c);
    fSchema2->collectTraits(c);
}

class seqSchema : public schema {
    schema *fSchema1;
    schema *fSchema2;
    double fHorzGap;

public:
    friend schema *makeSeqSchema(schema *s1, schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    seqSchema(schema *s1, schema *s2, double hgap);
    void drawInternalWires(device &dev);
    void collectInternalWires(collector &c);
};

enum { kHorDir, kUpDir, kDownDir };  ///< directions of connections

/**
 * Compute the direction of a connection. Note that
 * Y axis goes from top to bottom
 */
static int direction(const point &a, const point &b) {
    if (a.y > b.y) return kUpDir;    // upward connections
    if (a.y < b.y) return kDownDir;  // downward connection
    return kHorDir;                  // horizontal connections
}
/**
 * Compute the horizontal gap needed to draw the internal wires.
 * It depends on the largest group of connections that go in the same direction.
 */
static double computeHorzGap(schema *a, schema *b) {
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
schema *makeSeqSchema(schema *s1, schema *s2) {
    unsigned int o = s1->outputs;
    unsigned int i = s2->inputs;

    schema *a = (o < i) ? makeParSchema(s1, makeCableSchema(i - o)) : s1;
    schema *b = (o > i) ? makeParSchema(s2, makeCableSchema(o - i)) : s2;

    return new seqSchema(a, b, computeHorzGap(a, b));
}

/**
 * Constructor for a sequential schema (s1:s2).
 * The components s1 and s2 are supposed to be "compatible" (s1 : n->m and s2 : m->q).
 */
seqSchema::seqSchema(schema *s1, schema *s2, double hgap)
    : schema(s1->inputs, s2->outputs, s1->width + hgap + s2->width, max(s1->height, s2->height)),
      fSchema1(s1),
      fSchema2(s2),
      fHorzGap(hgap) {
    faustassert(s1->outputs == s2->inputs);
}

//-----------------------placement------------------------------

/**
 * Place the two components horizontally with enough space for the connections.
 */
void seqSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double y1 = max(0.0, 0.5 * (fSchema2->height - fSchema1->height));
    double y2 = max(0.0, 0.5 * (fSchema1->height - fSchema2->height));

    if (orientation == kLeftRight) {
        fSchema1->place(ox, oy + y1, orientation);
        fSchema2->place(ox + fSchema1->width + fHorzGap, oy + y2, orientation);
    } else {
        fSchema2->place(ox, oy + y2, orientation);
        fSchema1->place(ox + fSchema2->width + fHorzGap, oy + y1, orientation);
    }
    endPlace();
}

/**
 * The input points are the input points of the first component.
 */
point seqSchema::inputPoint(unsigned int i) const {
    return fSchema1->inputPoint(i);
}

/**
 * The output points are the output points of the second component.
 */
point seqSchema::outputPoint(unsigned int i) const {
    return fSchema2->outputPoint(i);
}

//--------------------------drawing------------------------------
/**
 * Draw the two components as well as the internal wires
 */
void seqSchema::draw(device &dev) {
    faustassert(placed());
    faustassert(fSchema1->outputs == fSchema2->inputs);

    fSchema1->draw(dev);
    fSchema2->draw(dev);
    // drawInternalWires(dev);
}

/**
 * Draw the two components as well as the internal wires
 */
void seqSchema::collectTraits(collector &c) {
    faustassert(placed());
    faustassert(fSchema1->outputs == fSchema2->inputs);

    fSchema1->collectTraits(c);
    fSchema2->collectTraits(c);
    collectInternalWires(c);
}

/**
 * Draw the internal wires aligning the vertical segments in
 * a symetric way when possible.
 */

void seqSchema::drawInternalWires(device &dev) {
    faustassert(fSchema1->outputs == fSchema2->inputs);

    const int N = fSchema1->outputs;
    double dx = 0;
    double mx = 0;
    int dir = -1;

    if (orientation == kLeftRight) {
        // draw left right cables
        for (int i = 0; i < N; i++) {
            point src = fSchema1->outputPoint(i);
            point dst = fSchema2->inputPoint(i);

            int d = direction(src, dst);
            if (d != dir) {
                // compute attributes of new direction
                switch (d) {
                    case kUpDir:mx = 0;
                        dx = dWire;
                        break;
                    case kDownDir:mx = fHorzGap;
                        dx = -dWire;
                        break;
                    default:mx = 0;
                        dx = 0;
                        break;
                }
                dir = d;
            } else {
                // move in same direction
                mx = mx + dx;
            }
            if (src.y == dst.y) {
                // draw straight cable
                dev.trait(src.x, src.y, dst.x, dst.y);
            } else {
                // draw zizag cable
                dev.trait(src.x, src.y, src.x + mx, src.y);
                dev.trait(src.x + mx, src.y, src.x + mx, dst.y);
                dev.trait(src.x + mx, dst.y, dst.x, dst.y);
            }
        }
    } else {
        // draw right left cables
        for (int i = 0; i < N; i++) {
            point src = fSchema1->outputPoint(i);
            point dst = fSchema2->inputPoint(i);

            int d = direction(src, dst);
            if (d != dir) {
                // compute attributes of new direction
                switch (d) {
                    case kUpDir:mx = -fHorzGap;
                        dx = dWire;
                        break;
                    case kDownDir:mx = 0;
                        dx = -dWire;
                        break;
                    default:mx = 0;
                        dx = 0;
                        break;
                }
                dir = d;
            } else {
                // move in same direction
                mx = mx + dx;
            }
            if (src.y == dst.y) {
                // draw straight cable
                dev.trait(src.x, src.y, dst.x, dst.y);
            } else {
                // draw zizag cable
                dev.trait(src.x, src.y, src.x + mx, src.y);
                dev.trait(src.x + mx, src.y, src.x + mx, dst.y);
                dev.trait(src.x + mx, dst.y, dst.x, dst.y);
            }
        }
    }
}

/**
 * Draw the internal wires aligning the vertical segments in
 * a symetric way when possible.
 */

void seqSchema::collectInternalWires(collector &c) {
    faustassert(fSchema1->outputs == fSchema2->inputs);

    const int N = fSchema1->outputs;
    double dx = 0;
    double mx = 0;
    int dir = -1;

    if (orientation == kLeftRight) {
        // draw left right cables
        for (int i = 0; i < N; i++) {
            point src = fSchema1->outputPoint(i);
            point dst = fSchema2->inputPoint(i);

            int d = direction(src, dst);
            if (d != dir) {
                // compute attributes of new direction
                switch (d) {
                    case kUpDir:mx = 0;
                        dx = dWire;
                        break;
                    case kDownDir:mx = fHorzGap;
                        dx = -dWire;
                        break;
                    default:mx = 0;
                        dx = 0;
                        break;
                }
                dir = d;
            } else {
                // move in same direction
                mx = mx + dx;
            }
            if (src.y == dst.y) {
                // draw straight cable
                c.addTrait(trait(point(src.x, src.y), point(dst.x, dst.y)));
            } else {
                // draw zizag cable
                c.addTrait(trait(point(src.x, src.y), point(src.x + mx, src.y)));
                c.addTrait(trait(point(src.x + mx, src.y), point(src.x + mx, dst.y)));
                c.addTrait(trait(point(src.x + mx, dst.y), point(dst.x, dst.y)));
            }
        }
    } else {
        // draw right left cables
        for (int i = 0; i < N; i++) {
            point src = fSchema1->outputPoint(i);
            point dst = fSchema2->inputPoint(i);

            int d = direction(src, dst);
            if (d != dir) {
                // compute attributes of new direction
                switch (d) {
                    case kUpDir:mx = -fHorzGap;
                        dx = dWire;
                        break;
                    case kDownDir:mx = 0;
                        dx = -dWire;
                        break;
                    default:mx = 0;
                        dx = 0;
                        break;
                }
                dir = d;
            } else {
                // move in same direction
                mx = mx + dx;
            }
            if (src.y == dst.y) {
                // draw straight cable
                c.addTrait(trait(point(src.x, src.y), point(dst.x, dst.y)));
            } else {
                // draw zizag cable
                c.addTrait(trait(point(src.x, src.y), point(src.x + mx, src.y)));
                c.addTrait(trait(point(src.x + mx, src.y), point(src.x + mx, dst.y)));
                c.addTrait(trait(point(src.x + mx, dst.y), point(dst.x, dst.y)));
            }
        }
    }
}

/**
 * Place and connect two diagrams in merge composition.
 */
class mergeSchema : public schema {
    schema *fSchema1;
    schema *fSchema2;
    double fHorzGap;

public:
    friend schema *makeMergeSchema(schema *s1, schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    mergeSchema(schema *s1, schema *s2, double hgap);
};

/**
 * Creates a new merge schema.
 * Cables are enlarged to `dWire`.
 * The horizontal gap between the two subschema is such that the connections are not too sloppy.
 */
schema *makeMergeSchema(schema *s1, schema *s2) {
    // avoid ugly diagram by ensuring at least dWire width
    schema *a = makeEnlargedSchema(s1, dWire);
    schema *b = makeEnlargedSchema(s2, dWire);
    double hgap = (a->height + b->height) / 4;
    return new mergeSchema(a, b, hgap);
}

/**
 * Constructor for a merge schema s1 :> s2 where the outputs of s1 are merged to the inputs of s2.
 * The constructor is private in order to enforce the usage of `makeMergeSchema`.
 */
mergeSchema::mergeSchema(schema *s1, schema *s2, double hgap)
    : schema(s1->inputs, s2->outputs, s1->width + s2->width + hgap, max(s1->height, s2->height)), fSchema1(s1), fSchema2(s2), fHorzGap(hgap) {}

/**
 * Place the two subschema horizontaly, centered, with enough gap for the connections.
 */
void mergeSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dy1 = max(0.0, fSchema2->height - fSchema1->height) / 2.0;
    double dy2 = max(0.0, fSchema1->height - fSchema2->height) / 2.0;

    if (orientation == kLeftRight) {
        fSchema1->place(ox, oy + dy1, orientation);
        fSchema2->place(ox + fSchema1->width + fHorzGap, oy + dy2, orientation);
    } else {
        fSchema2->place(ox, oy + dy2, orientation);
        fSchema1->place(ox + fSchema2->width + fHorzGap, oy + dy1, orientation);
    }
    endPlace();
}

/**
 * The inputs of s1 :> s2 are the inputs of s1.
 */
point mergeSchema::inputPoint(unsigned int i) const { return fSchema1->inputPoint(i); }

/**
 * The outputs of s1 :> s2 are the outputs of s2.
 */
point mergeSchema::outputPoint(unsigned int i) const { return fSchema2->outputPoint(i); }

/**
 * Draw the two subschema and the connections between them.
 */
void mergeSchema::draw(device &dev) {
    faustassert(placed());

    // draw the two subdiagrams
    fSchema1->draw(dev);
    fSchema2->draw(dev);

#if 0
    unsigned int r = fSchema2->fInputs;
    faustassert(r>0);

    // draw the connections between them
    for (unsigned int i=0; i<fSchema1->fOutputs; i++) {
        point p = fSchema1->outputPoint(i);
        point q = fSchema2->inputPoint(i%r);
        dev.trait(p.x, p.y, q.x, q.y);
    }
#endif
}

/**
 * Draw the two subschema and the connections between them.
 */
void mergeSchema::collectTraits(collector &c) {
    faustassert(placed());

    // draw the two subdiagrams
    fSchema1->collectTraits(c);
    fSchema2->collectTraits(c);

    unsigned int r = fSchema2->inputs;
    faustassert(r > 0);

    // draw the connections between them
    for (unsigned int i = 0; i < fSchema1->outputs; i++) {
        point p = fSchema1->outputPoint(i);
        point q = fSchema2->inputPoint(i % r);
        c.addTrait(trait(p, q));
    }
}

/**
 * Place and connect two diagrams in split composition.
 */
class splitSchema : public schema {
    schema *fSchema1;
    schema *fSchema2;
    double fHorzGap;

public:
    friend schema *makeSplitSchema(schema *s1, schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    splitSchema(schema *s1, schema *s2, double hgap);
};

/**
 * Creates a new split schema.
 * Cables are enlarged to `dWire`.
 * The horizontal gap between the two subschema is such that the connections are not too sloppy.
 */
schema *makeSplitSchema(schema *s1, schema *s2) {
    // make sure a and b are at least dWire large
    schema *a = makeEnlargedSchema(s1, dWire);
    schema *b = makeEnlargedSchema(s2, dWire);

    // horizontal gap to avoid too sloppy connections
    double hgap = (a->height + b->height) / 4;

    return new splitSchema(a, b, hgap);
}

/**
 * Constructor for a split schema s1 <: s2, where the outputs of s1 are distributed to the inputs of s2.
 * The constructor is private in order to enforce the usage of `makeSplitSchema`.
 */
splitSchema::splitSchema(schema *s1, schema *s2, double hgap)
    : schema(s1->inputs, s2->outputs, s1->width + s2->width + hgap, max(s1->height, s2->height)),
      fSchema1(s1),
      fSchema2(s2),
      fHorzGap(hgap) {
}

/**
 * Places the two subschema horizontaly, centered, with enough gap for
 * the connections
 */
void splitSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dy1 = max(0.0, fSchema2->height - fSchema1->height) / 2.0;
    double dy2 = max(0.0, fSchema1->height - fSchema2->height) / 2.0;

    if (orientation == kLeftRight) {
        fSchema1->place(ox, oy + dy1, orientation);
        fSchema2->place(ox + fSchema1->width + fHorzGap, oy + dy2, orientation);
    } else {
        fSchema2->place(ox, oy + dy2, orientation);
        fSchema1->place(ox + fSchema2->width + fHorzGap, oy + dy1, orientation);
    }
    endPlace();
}

/**
 * The inputs of s1 <: s2 are the inputs of s1.
 */
point splitSchema::inputPoint(unsigned int i) const { return fSchema1->inputPoint(i); }

/**
 * The outputs of s1 <: s2 are the outputs of s2.
 */
point splitSchema::outputPoint(unsigned int i) const { return fSchema2->outputPoint(i); }

/**
 * Draw the two sub schema and the connections between them.
 */
void splitSchema::draw(device &dev) {
    faustassert(placed());

    // draw the two subdiagrams
    fSchema1->draw(dev);
    fSchema2->draw(dev);

    unsigned int r = fSchema1->outputs;
    faustassert(r > 0);
#if 0
    // draw the connections between them
    for (unsigned int i=0; i<fSchema2->fInputs; i++) {
        point p = fSchema1->outputPoint(i%r);
        point q = fSchema2->inputPoint(i);
        if(p.z>0) {
            dev.trait(p.x, p.y, q.x, q.y);
        }
    }
#endif
}

/**
 * Draw the two subschema and the connections between them.
 */
void splitSchema::collectTraits(collector &c) {
    faustassert(placed());

    // draw the two subdiagrams
    fSchema1->collectTraits(c);
    fSchema2->collectTraits(c);

    unsigned int r = fSchema1->outputs;
    faustassert(r > 0);

    // draw the connections between them
    for (unsigned int i = 0; i < fSchema2->inputs; i++) {
        point p = fSchema1->outputPoint(i % r);
        point q = fSchema2->inputPoint(i);
        c.addTrait(trait(point(p.x, p.y), point(q.x, q.y)));
    }
}


/**
 * Place and connect two diagrams in recursive composition
 */
class recSchema : public schema {
    schema *fSchema1;
    schema *fSchema2;
    vector<point> fInputPoint;
    vector<point> fOutputPoint;

public:
    friend schema *makeRecSchema(schema *s1, schema *s2);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    recSchema(schema *s1, schema *s2, double width);
    void drawDelaySign(device &dev, double x, double y, double size);

    void collectFeedback(collector &c, const point &src, const point &dst, double dx, const point &out);
    void collectFeedfront(collector &c, const point &src, const point &dst, double dx);
};

/**
 * Creates a new recursive schema (s1 ~ s2).
 * The smallest component is enlarged to the width of the other.
 * The left and right horizontal margins are computed according to the number of internal connections.
 */
schema *makeRecSchema(schema *s1, schema *s2) {
    schema *a = makeEnlargedSchema(s1, s2->width);
    schema *b = makeEnlargedSchema(s2, s1->width);
    double m = dWire * max(b->inputs, b->outputs);
    double w = a->width + 2 * m;

    return new recSchema(a, b, w);
}

/**
 * Constructor of a recursive schema (s1 ~ s2).
 * The two components are supposed to have the same width.
 */
recSchema::recSchema(schema *s1, schema *s2, double width)
    : schema(s1->inputs - s2->outputs, s1->outputs, width, s1->height + s2->height),
      fSchema1(s1),
      fSchema2(s2) {
    // this version only accepts legal expressions of same width
    faustassert(s1->inputs >= s2->outputs);
    faustassert(s1->outputs >= s2->inputs);
    faustassert(s1->width >= s2->width);

    // create the input and output points
    for (unsigned int i = 0; i < inputs; i++) fInputPoint.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) fOutputPoint.emplace_back(0, 0);
}

/**
 * The two subschema are placed centered vertically, s2 on top of s1.
 * The input and output points are computed.
 */
void recSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    double dx1 = (width - fSchema1->width) / 2;
    double dx2 = (width - fSchema2->width) / 2;

    // place the two sub diagrams
    if (orientation == kLeftRight) {
        fSchema2->place(ox + dx2, oy, kRightLeft);
        fSchema1->place(ox + dx1, oy + fSchema2->height, kLeftRight);
    } else {
        fSchema1->place(ox + dx1, oy, kRightLeft);
        fSchema2->place(ox + dx2, oy + fSchema1->height, kLeftRight);
    }

    // adjust delta space to orientation
    if (orientation == kRightLeft) {
        dx1 = -dx1;
    }

    // place input points
    for (unsigned int i = 0; i < inputs; i++) {
        point p = fSchema1->inputPoint(i + fSchema2->outputs);
        fInputPoint[i] = point(p.x - dx1, p.y);
    }

    // place output points
    for (unsigned int i = 0; i < outputs; i++) {
        point p = fSchema1->outputPoint(i);
        fOutputPoint[i] = point(p.x + dx1, p.y);
    }

    endPlace();
}

/**
 * The input points s1 ~ s2
 */
point recSchema::inputPoint(unsigned int i) const { return fInputPoint[i]; }

/**
 * The output points s1 ~ s2
 */
point recSchema::outputPoint(unsigned int i) const { return fOutputPoint[i]; }

/**
 * Draw the two subschema s1 and s2 as well as the implicit feedback
 * delays between s1 and s2.
 */
void recSchema::draw(device &dev) {
    faustassert(placed());

    // draw the two subdiagrams
    fSchema1->draw(dev);
    fSchema2->draw(dev);

    // draw the implicit feedback delay to each fSchema2 input
    double dw = (orientation == kLeftRight) ? dWire : -dWire;
    for (unsigned int i = 0; i < fSchema2->inputs; i++) {
        const point &p = fSchema1->outputPoint(i);
        drawDelaySign(dev, p.x + i * dw, p.y, dw / 2);
    }
}

/**
 * Draw the delay sign of a feedback connection
 */
void recSchema::drawDelaySign(device &dev, double x, double y, double size) {
    dev.trait(x - size / 2, y, x - size / 2, y - size);
    dev.trait(x - size / 2, y - size, x + size / 2, y - size);
    dev.trait(x + size / 2, y - size, x + size / 2, y);
}

/**
 * Draw the two subschema s1 and s2 as well as the feedback
 * connections between s1 and s2, and the feedfrom connections
 * beetween s2 and s1.
 */
void recSchema::collectTraits(collector &c) {
    faustassert(placed());

    // draw the two subdiagrams
    fSchema1->collectTraits(c);
    fSchema2->collectTraits(c);

    // draw the feedback connections to each fSchema2 input
    for (unsigned int i = 0; i < fSchema2->inputs; i++) {
        collectFeedback(c, fSchema1->outputPoint(i), fSchema2->inputPoint(i), i * dWire, outputPoint(i));
    }

    // draw the non recursive output lines
    for (unsigned int i = fSchema2->inputs; i < outputs; i++) {
        point p = fSchema1->outputPoint(i);
        point q = outputPoint(i);
        c.addTrait(trait(p, q));  // in->out order
    }

    // draw the input lines
    unsigned int skip = fSchema2->outputs;
    for (unsigned int i = 0; i < inputs; i++) {
        point p = inputPoint(i);
        point q = fSchema1->inputPoint(i + skip);
        c.addTrait(trait(p, q));  // in->out order
    }

    // draw the feedfront connections from each fSchema2 output
    for (unsigned int i = 0; i < fSchema2->outputs; i++) {
        collectFeedfront(c, fSchema2->outputPoint(i), fSchema1->inputPoint(i), i * dWire);
    }
}

/**
 * Draw a feedback connection between two points with an horizontal
 * displacement dx
 */
void recSchema::collectFeedback(collector &c, const point &src, const point &dst, double dx, const point &out) {
    double ox = src.x + ((orientation == kLeftRight) ? dx : -dx);
    double ct = (orientation == kLeftRight) ? dWire / 2 : -dWire / 2;

    point up(ox, src.y - ct);
    point br(ox + ct / 2.0, src.y);

    c.addOutput(up);
    c.addOutput(br);
    c.addInput(br);

    c.addTrait(trait(up, point(ox, dst.y)));
    c.addTrait(trait(point(ox, dst.y), point(dst.x, dst.y)));
    c.addTrait(trait(src, br));
    c.addTrait(trait(br, out));
}

/**
 * Draw a feedfrom connection between two points with an horizontal
 * displacement dx
 */
void recSchema::collectFeedfront(collector &c, const point &src, const point &dst, double dx) {
    double ox = src.x + ((orientation == kLeftRight) ? -dx : dx);

    c.addTrait(trait(point(src.x, src.y), point(ox, src.y)));
    c.addTrait(trait(point(ox, src.y), point(ox, dst.y)));
    c.addTrait(trait(point(ox, dst.y), point(dst.x, dst.y)));
}

/**
 * A topSchema is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 * Arrows are added to all the outputs.
 */

class topSchema : public schema {
    schema *fSchema;
    double fMargin;
    string fText;
    string fLink;
    vector<point> fInputPoint;
    vector<point> fOutputPoint;

public:
    friend schema *makeTopSchema(schema *s1, double margin, const string &text, const string &link);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    topSchema(schema *s1, double margin, const string &text, const string &link);
};

/**
 * Creates a new top schema
 */
schema *makeTopSchema(schema *s, double margin, const string &text, const string &link) {
    return new topSchema(makeDecorateSchema(s, margin / 2, text), margin / 2, "", link);
}

/**
 * A topSchema is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 * Arrows are added to the outputs.
 * The constructor is made private to enforce the usage of `makeTopSchema`.
 */
topSchema::topSchema(schema *s, double margin, const string &text, const string &link)
    : schema(0, 0, s->width + 2 * margin, s->height + 2 * margin), fSchema(s), fMargin(margin), fText(text), fLink(link) {
}

/**
 * Define the graphic position of the schema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void topSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    fSchema->place(ox + fMargin, oy + fMargin, orientation);
    endPlace();
}

/**
 * Top schema has no input
 */
point topSchema::inputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < inputs);
    throw faustexception("ERROR : topSchema::inputPoint\n");
}

/**
 * Top schema has no output
 */
point topSchema::outputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < outputs);
    throw faustexception("ERROR : topSchema::outputPoint\n");
}

/**
 * Draw the enlarged schema. This methos can only
 * be called after the block have been placed
 */
void topSchema::draw(device &dev) {
    faustassert(placed());

    // draw a background white rectangle
    dev.rect(x, y, width - 1, height - 1, "#ffffff", fLink.c_str());
    // draw the label
    dev.label(x + fMargin, y + fMargin / 2, fText.c_str());

    fSchema->draw(dev);

    // draw arrows at output points of schema
    for (unsigned int i = 0; i < fSchema->outputs; i++) {
        point p = fSchema->outputPoint(i);
        dev.fleche(p.x, p.y, 0, orientation);
    }
}

/**
 * Draw the enlarged schema. This method can only
 * be called after the block have been placed
 */
void topSchema::collectTraits(collector &c) {
    faustassert(placed());
    fSchema->collectTraits(c);

    // draw arrows at output points of schema
    for (unsigned int i = 0; i < fSchema->inputs; i++) {
        point p = fSchema->inputPoint(i);
        c.addOutput(p);
    }

    // draw arrows at output points of schema
    for (unsigned int i = 0; i < fSchema->outputs; i++) {
        point p = fSchema->outputPoint(i);
        c.addInput(p);
    }
}

/**
 * A `decorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 */
class decorateSchema : public schema {
    schema *fSchema;
    double fMargin;
    string fText;
    vector<point> fInputPoint;
    vector<point> fOutputPoint;

public:
    friend schema *makeDecorateSchema(schema *s1, double margin, const string &text);

    void place(double ox, double oy, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

private:
    decorateSchema(schema *s1, double margin, const string &text);
};

/**
 * Creates a new decorated schema
 */
schema *makeDecorateSchema(schema *s, double margin, const string &text) { return new decorateSchema(s, margin, text); }

/**
 * A decorateSchema is a schema surrounded by a dashed rectangle with a label on the top left.
 * The rectangle is placed at half the margin parameter.
 * The constructor is made private to enforce the usage of `makeDecorateSchema`
 */
decorateSchema::decorateSchema(schema *s, double margin, const string &text)
    : schema(s->inputs, s->outputs, s->width + 2 * margin, s->height + 2 * margin), fSchema(s), fMargin(margin), fText(text) {
    for (unsigned int i = 0; i < inputs; i++) fInputPoint.emplace_back(0, 0);
    for (unsigned int i = 0; i < outputs; i++) fOutputPoint.emplace_back(0, 0);
}

/**
 * Define the graphic position of the schema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void decorateSchema::place(double ox, double oy, int orientation) {
    beginPlace(ox, oy, orientation);

    fSchema->place(ox + fMargin, oy + fMargin, orientation);

    double m = fMargin;
    if (orientation == kRightLeft) m = -m;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fSchema->inputPoint(i);
        fInputPoint[i] = point(p.x - m, p.y);  //, p.z);
    }

    for (unsigned int i = 0; i < outputs; i++) {
        point p = fSchema->outputPoint(i);
        fOutputPoint[i] = point(p.x + m, p.y);  //, p.z);
    }

    endPlace();
}

/**
 * Returns an input point
 */
point decorateSchema::inputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < inputs);
    return fInputPoint[i];
}

/**
 * Returns an output point
 */
point decorateSchema::outputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < outputs);
    return fOutputPoint[i];
}

/**
 * Draw the enlarged schema. This methods can only
 * be called after the block have been placed
 */
void decorateSchema::draw(device &dev) {
    faustassert(placed());

    fSchema->draw(dev);
#if 0
    // draw enlarge input wires
    for (unsigned int i=0; i<fInputs; i++) {
        point p = inputPoint(i);
        point q = fSchema->inputPoint(i);
        dev.trait(p.x, p.y, q.x, q.y);
    }

    // draw enlarge output wires
    for (unsigned int i=0; i<fOutputs; i++) {
        point p = outputPoint(i);
        point q = fSchema->outputPoint(i);
        dev.trait(p.x, p.y, q.x, q.y);
    }
#endif
    // define the coordinates of the frame
    double tw = (2 + fText.size()) * dLetter * 0.75;
    double x0 = x + fMargin / 2;             // left
    double y0 = y + fMargin / 2;             // top
    double x1 = x + width - fMargin / 2;   // right
    double y1 = y + height - fMargin / 2;  // bottom
    // double tl = x0 + 2*dWire;					// left of text zone
    double tl = x + fMargin;     // left of text zone
    double tr = min(tl + tw, x1);  // right of text zone

    // draw the surronding frame
    dev.dasharray(x0, y0, x0, y1);  // left line
    dev.dasharray(x0, y1, x1, y1);  // bottom line
    dev.dasharray(x1, y1, x1, y0);  // right line
    dev.dasharray(x0, y0, tl, y0);  // top segment before text
    dev.dasharray(tr, y0, x1, y0);  // top segment after text

    // draw the label
    dev.label(tl, y0, fText.c_str());  //
}

/**
 * Draw the enlarged schema. This methods can only
 * be called after the block have been placed
 */
void decorateSchema::collectTraits(collector &c) {
    faustassert(placed());

    fSchema->collectTraits(c);

    // draw enlarge input wires
    for (unsigned int i = 0; i < inputs; i++) {
        point p = inputPoint(i);
        point q = fSchema->inputPoint(i);
        c.addTrait(trait(p, q));  // in->out direction
    }

    // draw enlarge output wires
    for (unsigned int i = 0; i < outputs; i++) {
        point p = fSchema->outputPoint(i);
        point q = outputPoint(i);
        c.addTrait(trait(p, q));  // in->out direction
    }
}

/**
 * A simple rectangular box with a text and inputs and outputs.
 * The constructor is private in order to make sure `makeconnectorSchema` is used instead.
 */
class connectorSchema : public schema {
protected:
    // fields only defined after place() is called
    vector<point> fInputPoint;   ///< input connection points
    vector<point> fOutputPoint;  ///< output connection points

public:
    friend schema *makeConnectorSchema();

    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

protected:
    connectorSchema();

    void placeInputPoints();
    void placeOutputPoints();
    void collectInputWires(collector &c);
    void collectOutputWires(collector &c);
};

/**
 * Connectors are used to ensure unused inputs and outputs
 * are drawn
 */
schema *makeConnectorSchema() { return new connectorSchema(); }

/**
 * A connector is an invisible square fo dWire size
 * with 1 input and 1 output
 */
connectorSchema::connectorSchema() : schema(1, 1, dWire, dWire) {
    fInputPoint.emplace_back(0, 0);
    fOutputPoint.emplace_back(0, 0);
}

/**
 * Define the graphic position of the connectorSchema.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void connectorSchema::place(double x, double y, int orientation) {
    beginPlace(x, y, orientation);

    placeInputPoints();
    placeOutputPoints();

    endPlace();
}

/**
 * Returns an input point
 */
point connectorSchema::inputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < inputs);
    return fInputPoint[i];
}

/**
 * Returns an output point
 */
point connectorSchema::outputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < outputs);
    return fOutputPoint[i];
}

/**
 * Computes the input points according to the position and the
 * orientation of the connectorSchema
 */
void connectorSchema::placeInputPoints() {
    int N = inputs;

    if (orientation == kLeftRight) {
        double px = x;
        double py = y + (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fInputPoint[i] = point(px, py + i * dWire);
        }
    } else {
        double px = x + width;
        double py = y + height - (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fInputPoint[i] = point(px, py - i * dWire);
        }
    }
}

/**
 * Computes the output points according to the position and the
 * orientation of the connectorSchema
 */
void connectorSchema::placeOutputPoints() {
    int N = outputs;

    if (orientation == kLeftRight) {
        double px = x + width;
        double py = y + (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fOutputPoint[i] = point(px, py + i * dWire);
        }

    } else {
        double px = x;
        double py = y + height - (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fOutputPoint[i] = point(px, py - i * dWire);
        }
    }
}

/**
 * Draw the connectorSchema on the device.
 * This method can only be called after the `connectorSchema` has been placed.
 */
void connectorSchema::draw(device &) { faustassert(placed()); }

/**
 * Draw horizontal arrows from the input points to the
 * connectorSchema rectangle
 */
void connectorSchema::collectTraits(collector &c) {
    collectInputWires(c);
    collectOutputWires(c);
}

/**
 * Draw horizontal arrows from the input points to the
 * connectorSchema rectangle
 */
void connectorSchema::collectInputWires(collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fInputPoint[i];
        c.addTrait(trait(point(p.x, p.y), point(p.x + dx, p.y)));  // in->out direction
        c.addInput(point(p.x + dx, p.y));
    }
}

/**
 * Draw horizontal line from the connectorSchema rectangle to the
 * output points
 */
void connectorSchema::collectOutputWires(collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < outputs; i++) {
        point p = fOutputPoint[i];
        c.addTrait(trait(point(p.x - dx, p.y), point(p.x, p.y)));  // in->out direction
        c.addOutput(point(p.x - dx, p.y));
    }
}

/**
 * A simple rectangular box with a text and inputs and outputs.
 * The constructor is private in order to make sure `makeBlockSchema` is used instead.
 */
class routeSchema : public schema {
protected:
    const string fText;    ///< Text to be displayed
    const string fColor;   ///< color of the box
    const string fLink;    ///< option URL link
    const std::vector<int> fRoutes;  ///< route description: s1,d2,s2,d2,...

    // fields only defined after place() is called
    vector<point> fInputPoint;   ///< input connection points
    vector<point> fOutputPoint;  ///< output connection points

public:
    friend schema *makeRouteSchema(unsigned int n, unsigned int m, const std::vector<int> &routes);
    // friend schema* makeRoutingSchema(unsigned int inputs, unsigned int outputs, const vector<int>& route);
    void place(double x, double y, int orientation) override;
    void draw(device &dev) override;
    point inputPoint(unsigned int i) const override;
    point outputPoint(unsigned int i) const override;
    void collectTraits(collector &c) override;

protected:
    routeSchema(unsigned int inputs, unsigned int outputs, double width, double height, const std::vector<int> &routes);

    void placeInputPoints();
    void placeOutputPoints();

    void drawRectangle(device &dev);
    void drawText(device &dev);
    void drawOrientationMark(device &dev);
    void drawInputArrows(device &dev);
    //    void drawOutputWires(device& dev);

    void collectInputWires(collector &c);
    void collectOutputWires(collector &c);
};

/**
 * Build n x m cable routing
 */
schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const std::vector<int> &routes) {
    // determine the optimal size of the box
    double minimal = 3 * dWire;
    double h = 2 * dVert + max(minimal, max(inputs, outputs) * dWire);
    double w = 2 * dHorz + max(minimal, h * 0.75);

    return new routeSchema(inputs, outputs, w, h, routes);
}

/**
 * Build a simple colored `routeSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
 * The length of the text as well as the number of inputs and outputs are used to compute the size of the `routeSchema`
 */
routeSchema::routeSchema(unsigned int inputs, unsigned int outputs, double width, double height, const std::vector<int> &routes)
    : schema(inputs, outputs, width, height), fText(""), fColor("#EEEEAA"), fLink(""), fRoutes(routes) {
    for (unsigned int i = 0; i < inputs; i++) fInputPoint.emplace_back(point(0, 0));
    for (unsigned int i = 0; i < outputs; i++) fOutputPoint.emplace_back(0, 0);
}

/**
 * Define the graphic position of the `routeSchema`.
 * Computes the graphic position of all the elements, in particular the inputs and outputs.
 * This method must be called before `draw()`.
 */
void routeSchema::place(double x, double y, int orientation) {
    beginPlace(x, y, orientation);

    placeInputPoints();
    placeOutputPoints();

    endPlace();
}

/**
 * Returns an input point
 */
point routeSchema::inputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < inputs);
    return fInputPoint[i];
}

/**
 * Returns an output point
 */
point routeSchema::outputPoint(unsigned int i) const {
    faustassert(placed());
    faustassert(i < outputs);
    return fOutputPoint[i];
}

/**
 * Computes the input points according to the position and the orientation of the `routeSchema`.
 */
void routeSchema::placeInputPoints() {
    int N = inputs;

    if (orientation == kLeftRight) {
        double px = x;
        double py = y + (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fInputPoint[i] = point(px, py + i * dWire);
        }
    } else {
        double px = x + width;
        double py = y + height - (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fInputPoint[i] = point(px, py - i * dWire);
        }
    }
}

/**
 * Computes the output points according to the position and the orientation of the `routeSchema`.
 */
void routeSchema::placeOutputPoints() {
    int N = outputs;

    if (orientation == kLeftRight) {
        double px = x + width;
        double py = y + (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fOutputPoint[i] = point(px, py + i * dWire);
        }
    } else {
        double px = x;
        double py = y + height - (height - dWire * (N - 1)) / 2;

        for (int i = 0; i < N; i++) {
            fOutputPoint[i] = point(px, py - i * dWire);
        }
    }
}

/**
 * Draw the `routeSchema` on the device.
 * This method can only be called after the `routeSchema` have been placed.
 */
void routeSchema::draw(device &dev) {
    faustassert(placed());

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
void routeSchema::drawRectangle(device &dev) {
    dev.rect(x + dHorz, y + dVert, width - 2 * dHorz, height - 2 * dVert, fColor.c_str(), fLink.c_str());
}

/**
 * Draw the text centered on the box
 */
void routeSchema::drawText(device &dev) {
    dev.text(x + width / 2, y + height / 2, fText.c_str(), fLink.c_str());
}

/**
 * Draw the orientation mark, a small point that indicates
 * the first input (like integrated circuits)
 */
void routeSchema::drawOrientationMark(device &dev) {
    double px, py;

    if (orientation == kLeftRight) {
        px = x + dHorz;
        py = y + dVert;
    } else {
        px = x + width - dHorz;
        py = y + height - dVert;
    }

    dev.markSens(px, py, orientation);
}

/**
 * Draw horizontal arrows from the input points to the `routeSchema` rectangle.
 */
void routeSchema::drawInputArrows(device &dev) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fInputPoint[i];
        dev.fleche(p.x + dx, p.y, 0, orientation);
    }
}

/**
 * Draw horizontal arrows from the input points to the `routeSchema` rectangle.
 */
void routeSchema::collectTraits(collector &c) {
    collectInputWires(c);
    collectOutputWires(c);
    // additional routing traits
    for (unsigned int i = 0; i < fRoutes.size() - 1; i += 2) {
        int src = fRoutes[i] - 1;
        int dst = fRoutes[i + 1] - 1;
        point p1 = fInputPoint[src];
        point p2 = fOutputPoint[dst];
        // cerr << "add traits: " << p1.x << 'x' << p1.y << " -> " << p2.x << "x" << p2.y << endl;
        double dx = (orientation == kLeftRight) ? dHorz : -dHorz;
        c.addTrait(trait(point(p1.x + dx, p1.y), point(p2.x - dx, p2.y)));
    }
}

/**
 * Draw horizontal arrows from the input points to the `routeSchema` rectangle.
 */
void routeSchema::collectInputWires(collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < inputs; i++) {
        point p = fInputPoint[i];
        c.addTrait(trait(point(p.x, p.y), point(p.x + dx, p.y)));  // in->out direction
        c.addInput(point(p.x + dx, p.y));
    }
}

/**
 * Draw horizontal line from the `routeSchema` rectangle to the output points.
 */
void routeSchema::collectOutputWires(collector &c) {
    double dx = (orientation == kLeftRight) ? dHorz : -dHorz;

    for (unsigned int i = 0; i < outputs; i++) {
        point p = fOutputPoint[i];
        c.addTrait(trait(point(p.x - dx, p.y), point(p.x, p.y)));  // in->out direction
        c.addOutput(point(p.x - dx, p.y));
    }
}
