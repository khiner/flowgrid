#include "Schema.h"
#include "../../Helper/assert.h"
#include "imgui_internal.h"

using std::min;
using std::max;

// A simple rectangular box with a text and inputs and outputs.
struct BlockSchema : IOSchema {
    BlockSchema(unsigned int inputs, unsigned int outputs, float width, float height, string text, string color, string link);

    void drawImpl(Device &) const override;
    void collectLines() override;

protected:
    const string text;
    const string color;
    const string link;
};

// todo I don't understand this yet
static inline float quantize(int n) {
    static const int q = 3;
    return q * ((n + q - 1) / q); // NOLINT(bugprone-integer-division)
}

// Build a simple colored `BlockSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
// Computes the size of the box according to the length of the text and the maximum number of ports.
Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link) {
    const float minimal = 3 * dWire;
    const float w = 2 * dHorz + max(minimal, dLetter * quantize(int(text.size())));
    const float h = 2 * dVert + max(minimal, float(max(inputs, outputs)) * dWire);
    return new BlockSchema(inputs, outputs, w, h, text, color, link);
}

BlockSchema::BlockSchema(unsigned int inputs, unsigned int outputs, float width, float height, string text, string color, string link)
    : IOSchema(inputs, outputs, width, height), text(std::move(text)), color(std::move(color)), link(std::move(link)) {}

void BlockSchema::drawImpl(Device &device) const {
    device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
    device.text(ImVec2{x, y} + ImVec2{width, height} / 2, text.c_str(), link);

    // Draw a small point that indicates the first input (like an integrated circuits).
    const bool isLR = orientation == kLeftRight;
    device.dot(ImVec2{x, y} + (isLR ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

    // Input arrows
    for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR ? dHorz : -dHorz, 0}, 0, orientation);
}

// Input/output wires
void BlockSchema::collectLines() {
    const float dx = orientation == kLeftRight ? dHorz : -dHorz;
    for (const auto &p: inputPoints) lines.push_back({p, {p.x + dx, p.y}});
    for (const auto &p: outputPoints) lines.push_back({{p.x - dx, p.y}, p});
}

// Simple cables (identity box) in parallel.
// The width of a cable is null.
// Therefor, input and output connection points are the same.
struct CableSchema : Schema {
    CableSchema(unsigned int n);

    void placeImpl() override;
    ImVec2 inputPoint(unsigned int i) const override;
    ImVec2 outputPoint(unsigned int i) const override;

private:
    std::vector<ImVec2> points{inputs};
};

Schema *makeCableSchema(unsigned int n) { return new CableSchema(n); }

CableSchema::CableSchema(unsigned int n) : Schema(n, n, 0, float(n) * dWire) {}

// Place the communication points vertically spaced by `dWire`.
void CableSchema::placeImpl() {
    for (unsigned int i = 0; i < inputs; i++) {
        const float dx = dWire * (float(i) + 0.5f);
        points[i] = {x, y + (orientation == kLeftRight ? dx : height - dx)};
    }
}

ImVec2 CableSchema::inputPoint(unsigned int i) const { return points[i]; }
ImVec2 CableSchema::outputPoint(unsigned int i) const { return points[i]; }

// An inverter is a special symbol corresponding to '*(-1)' to create more compact diagrams.
struct InverterSchema : BlockSchema {
    InverterSchema(const string &color);
    void drawImpl(Device &) const override;
};

Schema *makeInverterSchema(const string &color) { return new InverterSchema(color); }

InverterSchema::InverterSchema(const string &color) : BlockSchema(1, 1, 2.5f * dWire, dWire, "-1", color, "") {}

void InverterSchema::drawImpl(Device &device) const {
    device.triangle({x + dHorz, y + 0.5f}, {width - 2 * dHorz, height - 1}, color, orientation, link);
}

// Terminate a cable (cut box).
struct CutSchema : Schema {
    CutSchema();

    void placeImpl() override;
    void drawImpl(Device &) const override;
    ImVec2 inputPoint(unsigned int i) const override;
    ImVec2 outputPoint(unsigned int i) const override;

private:
    ImVec2 point;
};

Schema *makeCutSchema() { return new CutSchema(); }

// A Cut is represented by a small black dot.
// It has 1 input and no outputs.
// It has a 0 width and a 1 wire height.
CutSchema::CutSchema() : Schema(1, 0, 0, dWire / 100.0f), point(0, 0) {}

// The input point is placed in the middle.
void CutSchema::placeImpl() {
    point = {x, y + height * 0.5f};
}

// A cut is represented by a small black dot.
void CutSchema::drawImpl(Device &) const {
//    device.circle(point, dWire / 8.0);
}

// By definition, a Cut has only one input point.
ImVec2 CutSchema::inputPoint(unsigned int) const { return point; }

// By definition, a Cut has no output point.
ImVec2 CutSchema::outputPoint(unsigned int) const {
    fgassert(false);
    return {-1, -1};
}

struct EnlargedSchema : IOSchema {
    EnlargedSchema(Schema *s, float width);

    void placeImpl() override;
    void drawImpl(Device &) const override;
    void collectLines() override;

private:
    Schema *schema;
};

// Returns an enlarged schema, but only if really needed.
// That is, if the required width is greater that the schema width.
Schema *makeEnlargedSchema(Schema *s, float width) {
    return width > s->width ? new EnlargedSchema(s, width) : s;
}

EnlargedSchema::EnlargedSchema(Schema *s, float width) : IOSchema(s->inputs, s->outputs, width, s->height), schema(s) {}

void EnlargedSchema::placeImpl() {
    float dx = (width - schema->width) / 2;
    schema->place(x + dx, y, orientation);

    if (orientation == kRightLeft) dx = -dx;

    for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - ImVec2{dx, 0.0f};
    for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + ImVec2{dx, 0};
}

void EnlargedSchema::drawImpl(Device &device) const { schema->draw(device); }

void EnlargedSchema::collectLines() {
    schema->collectLines();
    for (unsigned int i = 0; i < inputs; i++) lines.emplace_back(inputPoint(i), schema->inputPoint(i));
    for (unsigned int i = 0; i < outputs; i++) lines.emplace_back(schema->outputPoint(i), outputPoint(i));
}

struct ParallelSchema : BinarySchema {
    ParallelSchema(Schema *s1, Schema *s2);

    void placeImpl() override;
    ImVec2 inputPoint(unsigned int i) const override;
    ImVec2 outputPoint(unsigned int i) const override;

private:
    unsigned int inputFrontier;
    unsigned int outputFrontier;
};

// Make sure s1 and s2 have same width.
Schema *makeParallelSchema(Schema *s1, Schema *s2) {
    return new ParallelSchema(makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width));
}

ParallelSchema::ParallelSchema(Schema *s1, Schema *s2)
    : BinarySchema(s1, s2, s1->inputs + s2->inputs, s1->outputs + s2->outputs, s1->width, s1->height + s2->height),
      inputFrontier(s1->inputs), outputFrontier(s1->outputs) {
    fgassert(s1->width == s2->width);
}

void ParallelSchema::placeImpl() {
    if (orientation == kLeftRight) {
        schema1->place(x, y, orientation);
        schema2->place(x, y + schema1->height, orientation);
    } else {
        schema2->place(x, y, orientation);
        schema1->place(x, y + schema2->height, orientation);
    }
}

ImVec2 ParallelSchema::inputPoint(unsigned int i) const {
    return i < inputFrontier ? schema1->inputPoint(i) : schema2->inputPoint(i - inputFrontier);
}

ImVec2 ParallelSchema::outputPoint(unsigned int i) const {
    return i < outputFrontier ? schema1->outputPoint(i) : schema2->outputPoint(i - outputFrontier);
}

struct SequentialSchema : BinarySchema {
    SequentialSchema(Schema *s1, Schema *s2, float hgap);

    void placeImpl() override;
    void collectLines() override;

private:
    void collectInternalWires();

    float horzGap;
};

enum { kHorDir, kUpDir, kDownDir };  // directions of connections

// Compute the direction of a connection. Note that
// Y axis goes from top to bottom
static int direction(const ImVec2 &a, const ImVec2 &b) {
    if (a.y > b.y) return kUpDir;    // upward connections
    if (a.y < b.y) return kDownDir;  // downward connection
    return kHorDir;                  // horizontal connections
}

// Compute the horizontal gap needed to draw the internal wires.
// It depends on the largest group of connections that go in the same direction.
static float computeHorzGap(Schema *a, Schema *b) {
    fgassert(a->outputs == b->inputs);

    if (a->outputs == 0) return 0;

    // place a and b to have valid connection points
    a->place(0, max(0.0f, 0.5f * (b->height - a->height)), kLeftRight);
    b->place(0, max(0.0f, 0.5f * (a->height - b->height)), kLeftRight);

    // todo simplify
    // init current group direction and size
    int gdir = direction(a->outputPoint(0), b->inputPoint(0));
    int gsize = 1;

    int MaxGroupSize[3] = {0, 0, 0}; // store the size of the largest group for each direction
    // analyze direction of remaining points
    for (unsigned int i = 1; i < a->outputs; i++) {
        int d = direction(a->outputPoint(i), b->inputPoint(i));
        if (d == gdir) {
            gsize++;
        } else {
            MaxGroupSize[gdir] = max(MaxGroupSize[gdir], gsize);
            gsize = 1;
            gdir = d;
        }
    }

    // update for last group
    if (gsize > MaxGroupSize[gdir]) MaxGroupSize[gdir] = gsize;

    // the gap required for the connections
    return dWire * float(max(MaxGroupSize[kUpDir], MaxGroupSize[kDownDir]));
}

// May add cables to ensure the internal connections are between the same number of outputs and inputs.
// Compute a horizontal gap based on the number of upward and downward connections.
Schema *makeSequentialSchema(Schema *s1, Schema *s2) {
    const unsigned int o = s1->outputs;
    const unsigned int i = s2->inputs;
    auto *a = o < i ? makeParallelSchema(s1, makeCableSchema(i - o)) : s1;
    auto *b = o > i ? makeParallelSchema(s2, makeCableSchema(o - i)) : s2;

    return new SequentialSchema(a, b, computeHorzGap(a, b));
}

// Constructor for a sequential schema (s1:s2).
// The components s1 and s2 are supposed to be "compatible" (s1 : n->m and s2 : m->q).
SequentialSchema::SequentialSchema(Schema *s1, Schema *s2, float hgap)
    : BinarySchema(s1, s2, s1->inputs, s2->outputs, s1->width + hgap + s2->width, max(s1->height, s2->height)), horzGap(hgap) {
    fgassert(s1->outputs == s2->inputs);
}

// Place the two components horizontally with enough space for the connections.
void SequentialSchema::placeImpl() {
    const float y1 = max(0.0f, 0.5f * (schema2->height - schema1->height));
    const float y2 = max(0.0f, 0.5f * (schema1->height - schema2->height));
    if (orientation == kLeftRight) {
        schema1->place(x, y + y1, orientation);
        schema2->place(x + schema1->width + horzGap, y + y2, orientation);
    } else {
        schema2->place(x, y + y2, orientation);
        schema1->place(x + schema2->width + horzGap, y + y1, orientation);
    }
}

void SequentialSchema::collectLines() {
    BinarySchema::collectLines();
    collectInternalWires();
}

// Draw the internal wires aligning the vertical segments in a symmetric way when possible.
void SequentialSchema::collectInternalWires() {
    const unsigned int N = schema1->outputs;
    fgassert(N == schema2->inputs);

    float dx = 0, mx = 0;
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
            lines.emplace_back(src, dst);
        } else {
            // Draw a zigzag cable by traversing half the distance between, taking a sharp turn, then turning back and finishing.
            lines.push_back({src, {src.x + mx, src.y}});
            lines.push_back({{src.x + mx, src.y}, {src.x + mx, dst.y}});
            lines.push_back({{src.x + mx, dst.y}, dst});
        }
    }
}

// Place and connect two diagrams in merge composition.
struct MergeSchema : BinarySchema {
    MergeSchema(Schema *s1, Schema *s2, float hgap);

    void placeImpl() override;
    void collectLines() override;

private:
    float horzGap;
};

// Cables are enlarged to `dWire`.
Schema *makeMergeSchema(Schema *s1, Schema *s2) {
    auto *a = makeEnlargedSchema(s1, dWire);
    auto *b = makeEnlargedSchema(s2, dWire);

    // Horizontal gap to avoid sloppy connections.
    return new MergeSchema(a, b, (a->height + b->height) / 4);
}

// Constructor for a merge schema s1 :> s2 where the outputs of s1 are merged to the inputs of s2.
MergeSchema::MergeSchema(Schema *s1, Schema *s2, float hgap)
    : BinarySchema(s1, s2, s1->inputs, s2->outputs, s1->width + s2->width + hgap, max(s1->height, s2->height)), horzGap(hgap) {}

// Place the two subschema horizontally, centered, with enough gap for the connections.
void MergeSchema::placeImpl() {
    const float dy1 = max(0.0f, schema2->height - schema1->height) / 2.0f;
    const float dy2 = max(0.0f, schema1->height - schema2->height) / 2.0f;
    if (orientation == kLeftRight) {
        schema1->place(x, y + dy1, orientation);
        schema2->place(x + schema1->width + horzGap, y + dy2, orientation);
    } else {
        schema2->place(x, y + dy2, orientation);
        schema1->place(x + schema2->width + horzGap, y + dy1, orientation);
    }
}

void MergeSchema::collectLines() {
    BinarySchema::collectLines();
    for (unsigned int i = 0; i < schema1->outputs; i++) lines.emplace_back(schema1->outputPoint(i), schema2->inputPoint(i % schema2->inputs));
}

// Place and connect two diagrams in split composition.
struct SplitSchema : BinarySchema {
    SplitSchema(Schema *s1, Schema *s2, float hgap);

    void placeImpl() override;
    void collectLines() override;

private:
    float horzGap;
};

// Cables are enlarged to `dWire`.
Schema *makeSplitSchema(Schema *s1, Schema *s2) {
    // Make sure `a` and `b` are at least `dWire` large.
    auto *a = makeEnlargedSchema(s1, dWire);
    auto *b = makeEnlargedSchema(s2, dWire);
    // Horizontal gap to avoid sloppy connections.
    return new SplitSchema(a, b, (a->height + b->height) / 4);
}

// Constructor for a split schema s1 <: s2, where the outputs of s1 are distributed to the inputs of s2.
SplitSchema::SplitSchema(Schema *s1, Schema *s2, float hgap)
    : BinarySchema(s1, s2, s1->inputs, s2->outputs, s1->width + s2->width + hgap, max(s1->height, s2->height)), horzGap(hgap) {}

// Place the two subschema horizontally, centered, with enough gap for the connections
void SplitSchema::placeImpl() {
    const float dy1 = max(0.0f, schema2->height - schema1->height) / 2.0f;
    const float dy2 = max(0.0f, schema1->height - schema2->height) / 2.0f;
    if (orientation == kLeftRight) {
        schema1->place(x, y + dy1, orientation);
        schema2->place(x + schema1->width + horzGap, y + dy2, orientation);
    } else {
        schema2->place(x, y + dy2, orientation);
        schema1->place(x + schema2->width + horzGap, y + dy1, orientation);
    }
}

void SplitSchema::collectLines() {
    BinarySchema::collectLines();
    for (unsigned int i = 0; i < schema2->inputs; i++) lines.emplace_back(schema1->outputPoint(i % schema1->outputs), schema2->inputPoint(i));
}

// Place and connect two diagrams in recursive composition
struct RecSchema : IOSchema {
    RecSchema(Schema *s1, Schema *s2, float width);

    void placeImpl() override;
    void drawImpl(Device &) const override;
    void collectLines() override;

private:
    void collectFeedback(const ImVec2 &src, const ImVec2 &dst, float dx, const ImVec2 &out);
    void collectFeedfront(const ImVec2 &src, const ImVec2 &dst, float dx);

    Schema *schema1;
    Schema *schema2;
};

// Creates a new recursive schema (s1 ~ s2).
// The smallest component is enlarged to the width of the other.
Schema *makeRecSchema(Schema *s1, Schema *s2) {
    auto *a = makeEnlargedSchema(s1, s2->width);
    auto *b = makeEnlargedSchema(s2, s1->width);
    const float w = a->width + 2 * (dWire * max(float(b->inputs), float(b->outputs)));

    return new RecSchema(a, b, w);
}

// Constructor of a recursive schema (s1 ~ s2).
// The two components are supposed to have the same width.
RecSchema::RecSchema(Schema *s1, Schema *s2, float width)
    : IOSchema(s1->inputs - s2->outputs, s1->outputs, width, s1->height + s2->height), schema1(s1), schema2(s2) {
    // This version only accepts legal expressions of same width.
    fgassert(s1->inputs >= s2->outputs);
    fgassert(s1->outputs >= s2->inputs);
    fgassert(s1->width >= s2->width);
}

// The two subschema are placed centered vertically, s2 on top of s1.
void RecSchema::placeImpl() {
    float dx1 = (width - schema1->width) / 2;
    const float dx2 = (width - schema2->width) / 2;
    if (orientation == kLeftRight) {
        schema2->place(x + dx2, y, kRightLeft);
        schema1->place(x + dx1, y + schema2->height, kLeftRight);
    } else {
        schema1->place(x + dx1, y, kRightLeft);
        schema2->place(x + dx2, y + schema1->height, kLeftRight);
    }

    const ImVec2 d1 = {orientation == kRightLeft ? -dx1 : dx1, 0};
    for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema1->inputPoint(i + schema2->outputs) - d1;
    for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema1->outputPoint(i) + d1;
}

// Draw the delay sign of a feedback connection
static void drawDelaySign(Device &device, float x, float y, float size) {
    device.line({{x - size / 2, y}, {x - size / 2, y - size}});
    device.line({{x - size / 2, y - size}, {x + size / 2, y - size}});
    device.line({{x + size / 2, y - size}, {x + size / 2, y}});
}

void RecSchema::drawImpl(Device &device) const {
    schema1->draw(device);
    schema2->draw(device);

    // Draw the implicit feedback delay to each schema2 input
    const float dw = orientation == kLeftRight ? dWire : -dWire;
    for (unsigned int i = 0; i < schema2->inputs; i++) {
        const auto &p = schema1->outputPoint(i) + ImVec2{float(i) * dw, 0};
        drawDelaySign(device, p.x, p.y, dw / 2);
    }
}

void RecSchema::collectLines() {
    schema1->collectLines();
    schema2->collectLines();

    // Feedback connections to each schema2 input
    for (unsigned int i = 0; i < schema2->inputs; i++) collectFeedback(schema1->outputPoint(i), schema2->inputPoint(i), float(i) * dWire, outputPoint(i));

    // Non-recursive output lines
    for (unsigned int i = schema2->inputs; i < outputs; i++) lines.emplace_back(schema1->outputPoint(i), outputPoint(i));

    // Input lines
    const unsigned int skip = schema2->outputs;
    for (unsigned int i = 0; i < inputs; i++) lines.emplace_back(inputPoint(i), schema1->inputPoint(i + skip));

    // Feedfront connections from each schema2 output
    for (unsigned int i = 0; i < schema2->outputs; i++) collectFeedfront(schema2->outputPoint(i), schema1->inputPoint(i), float(i) * dWire);
}

// Draw a feedback connection between two points with a horizontal displacement `dx`.
void RecSchema::collectFeedback(const ImVec2 &src, const ImVec2 &dst, float dx, const ImVec2 &out) {
    const float ox = src.x + (orientation == kLeftRight ? dx : -dx);
    const float ct = (orientation == kLeftRight ? dWire : -dWire) / 2.0f;
    const ImVec2 up(ox, src.y - ct);
    const ImVec2 br(ox + ct / 2.0f, src.y);

    lines.push_back({up, {ox, dst.y}});
    lines.push_back({{ox, dst.y}, dst});
    lines.emplace_back(src, br);
    lines.emplace_back(br, out);
}

// Draw a feedfrom connection between two points with a horizontal displacement `dx`.
void RecSchema::collectFeedfront(const ImVec2 &src, const ImVec2 &dst, float dx) {
    const float ox = src.x + (orientation == kLeftRight ? -dx : dx);
    lines.push_back({{src.x, src.y}, {ox, src.y}});
    lines.push_back({{ox, src.y}, {ox, dst.y}});
    lines.push_back({{ox, dst.y}, {dst.x, dst.y}});
}

// A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left.
// The rectangle is placed at half the margin parameter.
// Arrows are added to all the outputs.
struct TopSchema : Schema {
    TopSchema(Schema *s1, float margin, string text, string link);

    void placeImpl() override;
    void drawImpl(Device &) const override;
    ImVec2 inputPoint(unsigned int i) const override;
    ImVec2 outputPoint(unsigned int i) const override;
    void collectLines() override;

private:
    Schema *schema;
    float fMargin;
    string text;
    string link;
};

Schema *makeTopSchema(Schema *s, float margin, const string &text, const string &link) {
    return new TopSchema(makeDecorateSchema(s, margin / 2, text), margin / 2, "", link);
}

// A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left.
// The rectangle is placed at half the margin parameter.
// Arrows are added to the outputs.
TopSchema::TopSchema(Schema *s, float margin, string text, string link)
    : Schema(0, 0, s->width + 2 * margin, s->height + 2 * margin), schema(s), fMargin(margin), text(std::move(text)), link(std::move(link)) {}

void TopSchema::placeImpl() {
    schema->place(x + fMargin, y + fMargin, orientation);
}

// Top schema has no input or output
ImVec2 TopSchema::inputPoint(unsigned int) const { throw std::runtime_error("ERROR : TopSchema::inputPoint"); }
ImVec2 TopSchema::outputPoint(unsigned int) const { throw std::runtime_error("ERROR : TopSchema::outputPoint"); }

void TopSchema::drawImpl(Device &device) const {
    device.rect({x, y, width - 1, height - 1}, "#ffffff", link);
    device.label(ImVec2{x, y} + ImVec2{fMargin, fMargin / 2}, text.c_str());

    schema->draw(device);

    for (unsigned int i = 0; i < schema->outputs; i++) device.arrow(schema->outputPoint(i), 0, orientation);
}

void TopSchema::collectLines() {
    schema->collectLines();
}

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left.
// The rectangle is placed at half the margin parameter.
struct DecorateSchema : IOSchema {
    DecorateSchema(Schema *s1, float margin, string text);

    void placeImpl() override;
    void drawImpl(Device &) const override;
    void collectLines() override;

private:
    Schema *schema;
    float margin;
    string text;
};

Schema *makeDecorateSchema(Schema *s, float margin, const string &text) { return new DecorateSchema(s, margin, text); }

// A DecorateSchema is a schema surrounded by a dashed rectangle with a label on the top left.
// The rectangle is placed at half the margin parameter.
DecorateSchema::DecorateSchema(Schema *s, float margin, string text)
    : IOSchema(s->inputs, s->outputs, s->width + 2 * margin, s->height + 2 * margin), schema(s), margin(margin), text(std::move(text)) {}

void DecorateSchema::placeImpl() {
    schema->place(x + margin, y + margin, orientation);

    const float m = orientation == kRightLeft ? -margin : margin;
    for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - ImVec2{m, 0};
    for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + ImVec2{m, 0};
}

void DecorateSchema::drawImpl(Device &device) const {
    schema->draw(device);

    // Surrounding frame
    const auto topLeft = ImVec2{x, y} + ImVec2{margin / 2, margin / 2};
    const auto topRight = topLeft + ImVec2{width - margin, 0};
    const auto bottomLeft = topLeft + ImVec2{0, height - margin};
    const auto bottomRight = bottomLeft + ImVec2{width - margin, 0};
    const float textLeft = x + margin;

    device.dasharray({topLeft, bottomLeft}); // left line
    device.dasharray({bottomLeft, bottomRight}); // bottom line
    device.dasharray({bottomRight, topRight}); // right line
    device.dasharray({topLeft, {textLeft, topLeft.y}}); // top segment before text
    device.dasharray({{min(textLeft + float(2 + text.size()) * dLetter * 0.75f, bottomRight.x), topLeft.y}, {bottomRight.x, topLeft.y}}); // top segment after text

    device.label({textLeft, topLeft.y}, text.c_str());
}

void DecorateSchema::collectLines() {
    schema->collectLines();
    for (unsigned int i = 0; i < inputs; i++) lines.emplace_back(inputPoint(i), schema->inputPoint(i));
    for (unsigned int i = 0; i < outputs; i++) lines.emplace_back(schema->outputPoint(i), outputPoint(i));
}

// A simple rectangular box with a text and inputs and outputs.
struct ConnectorSchema : IOSchema {
    ConnectorSchema();
    void collectLines() override;
};

// Connectors are used to ensure unused inputs and outputs are drawn.
Schema *makeConnectorSchema() { return new ConnectorSchema(); }

// A connector is an invisible square for `dWire` size with 1 input and 1 output.
ConnectorSchema::ConnectorSchema() : IOSchema(1, 1, dWire, dWire) {}

// Input/output wires
void ConnectorSchema::collectLines() {
    const float dx = orientation == kLeftRight ? dHorz : -dHorz;
    for (const auto &p: inputPoints) lines.emplace_back(p, p + ImVec2{dx, 0});
    for (const auto &p: outputPoints) lines.emplace_back(p - ImVec2{dx, 0}, p);
}

// A simple rectangular box with a text and inputs and outputs.
struct RouteSchema : IOSchema {
    RouteSchema(unsigned int inputs, unsigned int outputs, float width, float height, std::vector<int> routes);

    void drawImpl(Device &) const override;
    void collectLines() override;

protected:
    const string text;
    const string color;
    const string link;
    const std::vector<int> routes;  // Route description: s1,d2,s2,d2,...
};

// Build n x m cable routing
Schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const std::vector<int> &routes) {
    const float minimal = 3 * dWire;
    const float h = 2 * dVert + max(minimal, max(float(inputs), float(outputs)) * dWire);
    const float w = 2 * dHorz + max(minimal, h * 0.75f);
    return new RouteSchema(inputs, outputs, w, h, routes);
}

// Build a simple colored `RouteSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
// The length of the text as well as the number of inputs and outputs are used to compute the size of the `RouteSchema`
RouteSchema::RouteSchema(unsigned int inputs, unsigned int outputs, float width, float height, std::vector<int> routes)
    : IOSchema(inputs, outputs, width, height), color("#EEEEAA"), routes(std::move(routes)) {}

void RouteSchema::drawImpl(Device &device) const {
    static bool drawRouteFrame = false; // todo provide toggle
    if (drawRouteFrame) {
        device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
        // device.text(x + width / 2, y + height / 2, text.c_str(), link);

        // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
        const bool isLR = orientation == kLeftRight;
        device.dot(ImVec2{x, y} + (isLR ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

        // Input arrows
        for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR ? dHorz : -dHorz, 0}, 0, orientation);
    }
}

void RouteSchema::collectLines() {
    const float dx = orientation == kLeftRight ? dHorz : -dHorz;
    // Input/output wires
    for (const auto &p: inputPoints) lines.emplace_back(p, p + ImVec2{dx, 0});
    for (const auto &p: outputPoints) lines.emplace_back(p - ImVec2{dx, 0}, p);

    // Route wires
    for (unsigned int i = 0; i < routes.size() - 1; i += 2) {
        lines.emplace_back(inputPoints[routes[i] - 1] + ImVec2{dx, 0}, outputPoints[routes[i + 1] - 1] - ImVec2{dx, 0});
    }
}
