#include <set>
#include <stack>
#include <filesystem>

#include "DrawBox.hh"
#include "Schema.h"
#include "SVGDevice.h"

#include "property.hh"
#include "boxes/ppbox.hh"
#include "faust/dsp/libfaust-box.h"
#include "faust/dsp/libfaust-signal.h"

#define linkcolor "#003366"
#define normalcolor "#4B71A1"
#define uicolor "#477881"
#define slotcolor "#47945E"
#define numcolor "#f44800"
#define invcolor "#ffffff"

using namespace std;

#define FAUST_PATH_MAX 1024

struct DrawContext {
    DrawContext() {
        pureRoutingProperty = new property<bool>();
        inverter[0] = boxSeq(boxPar(boxWire(), boxInt(-1)), boxPrim2(sigMul));
        inverter[1] = boxSeq(boxPar(boxInt(-1), boxWire()), boxPrim2(sigMul));
        inverter[2] = boxSeq(boxPar(boxWire(), boxReal(-1.0)), boxPrim2(sigMul));
        inverter[3] = boxSeq(boxPar(boxReal(-1.0), boxWire()), boxPrim2(sigMul));
        inverter[4] = boxSeq(boxPar(boxInt(0), boxWire()), boxPrim2(sigSub));
        inverter[5] = boxSeq(boxPar(boxReal(0.0), boxWire()), boxPrim2(sigSub));
    }

    Tree boxComplexityMemo{}; // Avoid recomputing box complexity
    string currentDir; // Save current directory name
    property<bool> *pureRoutingProperty;
    bool foldingFlag = false; // true with complex block-diagrams
    int foldThreshold = 25; // global complexity threshold before activating folding
    int foldComplexity = 2;  // individual complexity threshold before folding
    set<Tree> drawnExp; // Expressions drawn or scheduled so far
    map<Tree, string> backLink; // Link to enclosing file for sub schema
    stack<Tree> pendingExp; // Expressions that need to be drawn
    string schemaFileName;  // Name of schema file being generated
    Tree inverter[6]{};
};

std::unique_ptr<DrawContext> dc;

static int computeComplexity(Box box);

// Memoized version of `computeComplexity(Box)`
int boxComplexity(Box box) {
    Tree prop = box->getProperty(dc->boxComplexityMemo);
    if (prop) return tree2int(prop);

    int v = computeComplexity(box);
    box->setProperty(dc->boxComplexityMemo, tree(v));
    return v;
}

// Compute the complexity of a box expression tree according to the complexity of its subexpressions.
// Basically, it counts the number of boxes to be drawn.
// If the box-diagram expression is not evaluated, it will throw an error.
int computeComplexity(Box box) {
    if (isBoxCut(box) || isBoxWire(box)) return 0;

    int i;
    double r;
    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;

    const auto *xt = getUserData(box);

    // simple elements / slot
    if (xt ||
        isBoxInt(box, &i) ||
        isBoxReal(box, &r) ||
        isBoxWaveform(box) ||
        isBoxPrim0(box, &p0) ||
        isBoxPrim1(box, &p1) ||
        isBoxPrim2(box, &p2) ||
        isBoxPrim3(box, &p3) ||
        isBoxPrim4(box, &p4) ||
        isBoxPrim5(box, &p5) ||
        isBoxSlot(box, &i))
        return 1;

    Tree ff, type, name, file;
    // foreign elements
    if (isBoxFFun(box, ff) ||
        isBoxFConst(box, type, name, file) ||
        isBoxFVar(box, type, name, file))
        return 1;

    Tree t1, t2;

    // symbolic boxes
    if (isBoxSymbolic(box, t1, t2)) return 1 + boxComplexity(t2);

    // binary operators
    if (isBoxSeq(box, t1, t2) ||
        isBoxSplit(box, t1, t2) ||
        isBoxMerge(box, t1, t2) ||
        isBoxPar(box, t1, t2) ||
        isBoxRec(box, t1, t2))
        return boxComplexity(t1) + boxComplexity(t2);

    Tree label, cur, min, max, step, chan;

    // user interface widgets
    if (isBoxButton(box, label) ||
        isBoxCheckbox(box, label) ||
        isBoxVSlider(box, label, cur, min, max, step) ||
        isBoxHSlider(box, label, cur, min, max, step) ||
        isBoxHBargraph(box, label, min, max) ||
        isBoxVBargraph(box, label, min, max) ||
        isBoxSoundfile(box, label, chan) ||
        isBoxNumEntry(box, label, cur, min, max, step))
        return 1;

    // user interface groups
    if (isBoxVGroup(box, label, t1) ||
        isBoxHGroup(box, label, t1) ||
        isBoxTGroup(box, label, t1) ||
        isBoxMetadata(box, t1, t2))
        return boxComplexity(t1);

    Tree t3;
    // environment/route
    if (isBoxEnvironment(box) || isBoxRoute(box, t1, t2, t3)) return 0;

    stringstream error;
    error << "ERROR in boxComplexity : not an evaluated box [[ " << *box << " ]]\n";
    throw runtime_error(error.str());
}

namespace fs = std::filesystem;

void getCurrentDir() {
    char buffer[FAUST_PATH_MAX];
    char *current_dir = getcwd(buffer, FAUST_PATH_MAX);
    dc->currentDir = current_dir ? current_dir : "";
}

void mkchDir(const string &dirname) {
    getCurrentDir();
    if (!dc->currentDir.empty()) {
        fs::remove_all("FaustDiagrams");
        if (fs::create_directory(dirname) && chdir(dirname.c_str()) == 0) return;
    }

    throw std::runtime_error((stringstream("ERROR : mkchDir : ") << strerror(errno)).str());
}

void choldDir() {
    if (chdir(dc->currentDir.c_str()) != 0) throw std::runtime_error((stringstream("ERROR : choldDir : ") << strerror(errno)).str());
}

static Schema *createSchema(Tree t);

// Generate a 1->0 block schema for an input slot.
static Schema *generateInputSlotSchema(Tree a) {
    Tree id;
    getDefNameProperty(a, id);
    return makeBlockSchema(1, 0, tree2str(id), slotcolor, "");
}
// Generate an abstraction schema by placing in sequence the input slots and the body.
static Schema *generateAbstractionSchema(Schema *x, Tree t) {
    Tree a, b;
    while (isBoxSymbolic(t, a, b)) {
        x = makeParallelSchema(x, generateInputSlotSchema(a));
        t = b;
    }
    return makeSequentialSchema(x, createSchema(t));
}

static Schema *addSchemaInputs(int ins, Schema *x) {
    if (ins == 0) return x;

    Schema *y = nullptr;
    do {
        Schema *z = makeConnectorSchema();
        y = y != nullptr ? makeParallelSchema(y, z) : z;
    } while (--ins);

    return makeSequentialSchema(y, x);
}
static Schema *addSchemaOutputs(int outs, Schema *x) {
    if (outs == 0) return x;

    Schema *y = nullptr;
    do {
        Schema *z = makeConnectorSchema();
        y = y != nullptr ? makeParallelSchema(y, z) : z;
    } while (--outs);

    return makeSequentialSchema(x, y);
}

// Transform the definition name property of tree <t> into a legal file name.
// The resulting file name is stored in <dst> a table of at least <n> chars.
// Returns the <dst> pointer for convenience.
static char *legalFileName(Tree t, char *dst) {
    static int n = FAUST_PATH_MAX;
    Tree id;
    int i = 0;
    if (getDefNameProperty(t, id)) {
        const char *src = tree2str(id);
        for (i = 0; isalnum(src[i]) && i < 16; i++) {
            dst[i] = src[i];
        }
    }
    dst[i] = 0;
    if (strcmp(dst, "process") != 0) {
        // if it is not process add the hex address to make the name unique
        snprintf(&dst[i], n - i, "-%p", (void *) t);
    }
    return dst;
}

// Returns `true` if `t == '*(-1)'`.
// This test is used to simplify diagram by using a special symbol for inverters.
static bool isInverter(Tree t) {
    for (const auto &i: dc->inverter) if (t == i) return true;
    return false;
}

// Collect the leaf numbers of tree l into vector v.
// Return true if l a number or a parallel tree of numbers.
static bool isIntTree(Tree l, vector<int> &v) {
    int n;
    double r;
    Tree x, y;

    if (isBoxInt(l, &n)) {
        v.push_back(n);
        return true;
    }
    if (isBoxReal(l, &r)) {
        v.push_back(int(r));
        return true;
    }
    if (isBoxPar(l, x, y)) return isIntTree(x, v) && isIntTree(y, v);

    stringstream error;
    error << "ERROR in file " << __FILE__ << ':' << __LINE__ << ", not a valid list of numbers : " << boxpp(l) << endl;
    throw std::runtime_error(error.str());
}

// Convert user interface element into a textual representation
static string userInterfaceDescription(Tree box) {
    Tree t1, label, cur, min, max, step, chan;
    if (isBoxButton(box, label)) return "button(" + extractName(label) + ')';
    if (isBoxCheckbox(box, label)) return "checkbox(" + extractName(label) + ')';
    if (isBoxVSlider(box, label, cur, min, max, step)) return (stringstream("vslider(") << extractName(label) << ", " << boxpp(cur) << ", " << boxpp(min) << ", " << boxpp(max) << ", " << boxpp(step) << ')').str();
    if (isBoxHSlider(box, label, cur, min, max, step)) return (stringstream("hslider(") << extractName(label) << ", " << boxpp(cur) << ", " << boxpp(min) << ", " << boxpp(max) << ", " << boxpp(step) << ')').str();
    if (isBoxVGroup(box, label, t1)) return (stringstream("vgroup(") << extractName(label) << ", " << boxpp(t1, 0) << ')').str();
    if (isBoxHGroup(box, label, t1)) return (stringstream("hgroup(") << extractName(label) << ", " << boxpp(t1, 0) << ')').str();
    if (isBoxTGroup(box, label, t1)) return (stringstream("tgroup(") << extractName(label) << ", " << boxpp(t1, 0) << ')').str();
    if (isBoxHBargraph(box, label, min, max)) return (stringstream("hbargraph(") << extractName(label) << ", " << boxpp(min) << ", " << boxpp(max) << ')').str();
    if (isBoxVBargraph(box, label, min, max)) return (stringstream("vbargraph(") << extractName(label) << ", " << boxpp(min) << ", " << boxpp(max) << ')').str();
    if (isBoxNumEntry(box, label, cur, min, max, step)) return (stringstream("nentry(") << extractName(label) << ", " << boxpp(cur) << ", " << boxpp(min) << ", " << boxpp(max) << ", " << boxpp(step) << ')').str();
    if (isBoxSoundfile(box, label, chan)) return (stringstream("soundfile(") << extractName(label) << ", " << boxpp(chan) << ')').str();

    throw std::runtime_error("ERROR : unknown user interface element");
}

// Generate the inside schema of a block diagram according to its type.
static Schema *generateInsideSchema(Tree t) {
    if (getUserData(t) != nullptr) return makeBlockSchema(xtendedArity(t), 1, xtendedName(t), normalcolor, "");
    if (isInverter(t)) return makeInverterSchema(invcolor);

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) {
        stringstream s;
        if (isBoxInt(t)) s << i;
        else s << r;
        return makeBlockSchema(0, 1, s.str(), numcolor, "");
    }

    if (isBoxWaveform(t)) return makeBlockSchema(0, 2, "waveform{...}", normalcolor, "");
    if (isBoxWire(t)) return makeCableSchema();
    if (isBoxCut(t)) return makeCutSchema();

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return makeBlockSchema(0, 1, prim0name(p0), normalcolor, "");
    if (isBoxPrim1(t, &p1)) return makeBlockSchema(1, 1, prim1name(p1), normalcolor, "");
    if (isBoxPrim2(t, &p2)) return makeBlockSchema(2, 1, prim2name(p2), normalcolor, "");
    if (isBoxPrim3(t, &p3)) return makeBlockSchema(3, 1, prim3name(p3), normalcolor, "");
    if (isBoxPrim4(t, &p4)) return makeBlockSchema(4, 1, prim4name(p4), normalcolor, "");
    if (isBoxPrim5(t, &p5)) return makeBlockSchema(5, 1, prim5name(p5), normalcolor, "");

    Tree ff;
    if (isBoxFFun(t, ff)) return makeBlockSchema(ffarity(ff), 1, ffname(ff), normalcolor, "");

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return makeBlockSchema(0, 1, tree2str(name), normalcolor, "");
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return makeBlockSchema(0, 1, userInterfaceDescription(t), uicolor, "");
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return makeBlockSchema(1, 1, userInterfaceDescription(t), uicolor, "");
    if (isBoxSoundfile(t, label, chan)) return makeBlockSchema(2, 2 + tree2int(chan), userInterfaceDescription(t), uicolor, "");

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return createSchema(a);

    const bool isVGroup = isBoxVGroup(t, label, a);
    const bool isHGroup = isBoxHGroup(t, label, a);
    const bool isTGroup = isBoxTGroup(t, label, a);
    if (isVGroup || isHGroup || isTGroup) {
        const string groupId = isVGroup ? "v" : isHGroup ? "h" : "t";
        auto *s1 = createSchema(a);
        return makeDecorateSchema(s1, 10, groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return makeSequentialSchema(createSchema(a), createSchema(b));
    if (isBoxPar(t, a, b)) return makeParallelSchema(createSchema(a), createSchema(b));
    if (isBoxSplit(t, a, b)) return makeSplitSchema(createSchema(a), createSchema(b));
    if (isBoxMerge(t, a, b)) return makeMergeSchema(createSchema(a), createSchema(b));
    if (isBoxRec(t, a, b)) return makeRecSchema(createSchema(a), createSchema(b));
    if (isBoxSlot(t, &i)) {
        Tree id;
        getDefNameProperty(t, id);
        return makeBlockSchema(0, 1, tree2str(id), slotcolor, "");
    }
    if (isBoxSymbolic(t, a, b)) {
        auto *inputSlotSchema = generateInputSlotSchema(a);
        auto *abstractionSchema = generateAbstractionSchema(inputSlotSchema, b);
        Tree id;
        if (getDefNameProperty(t, id)) return abstractionSchema;
        return makeDecorateSchema(abstractionSchema, 10, "Abstraction");
    }
    if (isBoxEnvironment(t)) return makeBlockSchema(0, 0, "environment{...}", normalcolor, "");

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) return makeRouteSchema(ins, outs, route);

        stringstream error;
        error << "ERROR in file " << __FILE__ << ':' << __LINE__ << ", invalid route expression : " << boxpp(t) << endl;
        throw std::runtime_error(error.str());
    }

    throw std::runtime_error((stringstream("ERROR in generateInsideSchema, box expression not recognized: ") << boxpp(t)).str());
}
// Write a top level diagram.
// A top level diagram is decorated with its definition name property and is drawn in an individual file.
static void writeSchemaFile(Tree bd) {
    Tree id;
    Schema *ts;
    int ins, outs;

    char temp[FAUST_PATH_MAX];

    getBoxType(bd, &ins, &outs);

    bool hasname = getDefNameProperty(bd, id);
    if (!hasname) id = tree(Node(unique("diagram_"))); // create an arbitrary name

    // generate legal file name for the schema
    stringstream s1;
    s1 << legalFileName(bd, temp) << ".svg";
    string res1 = s1.str();
    dc->schemaFileName = res1;

    // generate the label of the schema
    string link = dc->backLink[bd];
    ts = makeTopSchema(addSchemaOutputs(outs, addSchemaInputs(ins, generateInsideSchema(bd))), 20, tree2str(id), link);

    SVGDevice dev(res1, ts->width, ts->height);
    // todo combine place/collect/draw
    ts->place(0, 0, kLeftRight);
    ts->collectLines();
    ts->draw(dev);
}

// Schedule a block diagram to be drawn.
static void scheduleDrawing(Tree t) {
    if (dc->drawnExp.find(t) == dc->drawnExp.end()) {
        dc->drawnExp.insert(t);
        dc->backLink.insert(make_pair(t, dc->schemaFileName));  // remember the enclosing filename
        dc->pendingExp.push(t);
    }
}

// Retrieve next block diagram that must be drawn.
static bool pendingDrawing(Tree &t) {
    if (dc->pendingExp.empty()) return false;
    t = dc->pendingExp.top();
    dc->pendingExp.pop();
    return true;
}

void drawBox(Box box) {
    dc = std::make_unique<DrawContext>();
    dc->foldingFlag = boxComplexity(box) > dc->foldThreshold;
    mkchDir("FaustDiagrams"); // create a directory to store files

    scheduleDrawing(box); // schedule the initial drawing

    Tree t;
    while (pendingDrawing(t)) {
        writeSchemaFile(t); // generate all the pending drawing
    }

    choldDir(); // return to original directory
}

// Compute the Pure Routing property.
// That is, expressions only made of cut, wires and slots.
// No labels will be displayed for pure routing expressions.
static bool isPureRouting(Tree t) {
    bool r;
    int ID;
    Tree x, y;

    if (dc->pureRoutingProperty->get(t, r)) return r;

    if (isBoxCut(t) || isBoxWire(t) || isInverter(t) || isBoxSlot(t, &ID) ||
        (isBoxPar(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxSeq(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxSplit(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxMerge(t, x, y) && isPureRouting(x) && isPureRouting(y))) {
        dc->pureRoutingProperty->set(t, true);
        return true;
    }

    dc->pureRoutingProperty->set(t, false);
    return false;
}

// Generate an appropriate schema according to the type of block diagram.
static Schema *createSchema(Tree t) {
    Tree id;
    int ins, outs;

    const bool hasname = getDefNameProperty(t, id);
    if (dc->foldingFlag && boxComplexity(t) >= dc->foldComplexity && hasname) {
        char temp[FAUST_PATH_MAX];
        getBoxType(t, &ins, &outs);
        stringstream l;
        l << legalFileName(t, temp) << ".svg";
        scheduleDrawing(t);
        return makeBlockSchema(ins, outs, tree2str(id), linkcolor, l.str());
    }

    // Not a slot, with a name. Draw a line around the object with its name.
    if (hasname && !isPureRouting(t)) return makeDecorateSchema(generateInsideSchema(t), 10, tree2str(id));

    return generateInsideSchema(t); // normal case
}
