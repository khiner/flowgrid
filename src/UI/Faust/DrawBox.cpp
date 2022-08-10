#include <map>
#include <sstream>
#include <stack>
#include <string>
#include <filesystem>

#include "DrawBox.hh"
#include "Schema.h"
#include "SVGDev.h"

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
string gCurrentDir; // Save current directory name
namespace fs = std::filesystem;

void getCurrentDir() {
    char buffer[FAUST_PATH_MAX];
    char *current_dir = getcwd(buffer, FAUST_PATH_MAX);
    gCurrentDir = current_dir ? current_dir : "";
}

void mkchDir(const string &dirname) {
    getCurrentDir();
    if (!gCurrentDir.empty()) {
        fs::remove_all("FaustDiagrams");
        if (fs::create_directory(dirname) && chdir(dirname.c_str()) == 0) {
            return;
        }
    }

    throw std::runtime_error((stringstream("ERROR : mkchDir : ") << strerror(errno)).str());
}

void choldDir() {
    if (chdir(gCurrentDir.c_str()) != 0) throw std::runtime_error((stringstream("ERROR : choldDir : ") << strerror(errno)).str());
}

void faustassertaux(bool cond, const string &file, int line) {
    if (!cond) {
        stringstream str;
        str << "file: " << file.substr(file.find_last_of('/') + 1) << ", line: " << line << ", ";
        str << "version: " << FAUSTVERSION;
        stacktrace(str, 20);
        throw faustexception(str.str());
    }
}

bool getProperty(Tree t, Tree key, Tree &val) {
    if (CTree *pl = t->getProperty(key)) {
        val = pl;
        return true;
    }

    return false;
}

bool getDefNameProperty(Tree t, Tree &id) {
    static Tree DEFNAMEPROPERTY = tree(symbol("DEFNAMEPROPERTY"));
    return getProperty(t, DEFNAMEPROPERTY, id);
}

/**
 * property Key used to store box complexity
 */

static int computeBoxComplexity(Tree box);

Tree BCOMPLEXITY; // Node used for memoization purposes

/**
 * Return the complexity property of a box expression tree.
 * If no complexity property exist, it is created and computeBoxComplexity
 * is called do to the job.
 *
 * @param box an evaluated box expression tree
 * @return the complexity of box
 *
 * @see computeBoxComplexity
 */
int boxComplexity(Tree box) {
    Tree prop = box->getProperty(BCOMPLEXITY);
    if (prop) return tree2int(prop);

    int v = computeBoxComplexity(box);
    box->setProperty(BCOMPLEXITY, tree(v));
    return v;
}

/**
 * internal shortcut to simplify computeBoxComplexity code
 */
#define BC boxComplexity

/**
 * Compute the complexity of a box expression.
 *
 * Compute the complexity of a box expression tree according to the
 * complexity of its subexpressions. Basically it counts the number
 * of boxes to be drawn. The box-diagram expression is supposed
 * to be evaluated. It will exit with an error if it is not the case.
 *
 * @param box an evaluated box expression tree
 * @return the complexity of box
 */
int computeBoxComplexity(Tree box) {
    int i;
    double r;
    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;

    Tree t1, t2, t3, ff, label, cur, min, max, step, type, name, file, chan;

    auto *xt = getUserData(box);

    // simple elements
    if (xt) return 1;
    if (isBoxInt(box, &i)) return 1;
    if (isBoxReal(box, &r)) return 1;
    if (isBoxWaveform(box)) return 1;
    if (isBoxCut(box)) return 0;
    if (isBoxWire(box)) return 0;
    if (isBoxPrim0(box, &p0)) return 1;
    if (isBoxPrim1(box, &p1)) return 1;
    if (isBoxPrim2(box, &p2)) return 1;
    if (isBoxPrim3(box, &p3)) return 1;
    if (isBoxPrim4(box, &p4)) return 1;
    if (isBoxPrim5(box, &p5)) return 1;

    // foreign elements
    if (isBoxFFun(box, ff)) return 1;
    if (isBoxFConst(box, type, name, file)) return 1;
    if (isBoxFVar(box, type, name, file)) return 1;

    // slots and symbolic boxes
    if (isBoxSlot(box, &i)) return 1;
    if (isBoxSymbolic(box, t1, t2)) return 1 + BC(t2);

    // block diagram binary operator
    if (isBoxSeq(box, t1, t2)) return BC(t1) + BC(t2);
    if (isBoxSplit(box, t1, t2)) return BC(t1) + BC(t2);
    if (isBoxMerge(box, t1, t2)) return BC(t1) + BC(t2);
    if (isBoxPar(box, t1, t2)) return BC(t1) + BC(t2);
    if (isBoxRec(box, t1, t2)) return BC(t1) + BC(t2);

    // user interface widgets
    if (isBoxButton(box, label)) return 1;
    if (isBoxCheckbox(box, label)) return 1;
    if (isBoxVSlider(box, label, cur, min, max, step)) return 1;
    if (isBoxHSlider(box, label, cur, min, max, step)) return 1;
    if (isBoxHBargraph(box, label, min, max)) return 1;
    if (isBoxVBargraph(box, label, min, max)) return 1;
    if (isBoxSoundfile(box, label, chan)) return 1;
    if (isBoxNumEntry(box, label, cur, min, max, step)) return 1;

    // user interface groups
    if (isBoxVGroup(box, label, t1)) return BC(t1);
    if (isBoxHGroup(box, label, t1)) return BC(t1);
    if (isBoxTGroup(box, label, t1)) return BC(t1);

    // environment
    if (isBoxEnvironment(box)) return 0;
    if (isBoxMetadata(box, t1, t2)) return BC(t1);
    if (isBoxRoute(box, t1, t2, t3)) return 0;

    // to complete
    stringstream error;
    error << "ERROR in boxComplexity : not an evaluated box [[ " << *box << " ]]\n";
    throw faustexception(error.str());
}

// prototypes of internal functions
static void writeSchemaFile(Tree bd);
static schema *generateDiagramSchema(Tree t);
static schema *generateInsideSchema(Tree t);
static void scheduleDrawing(Tree t);
static bool pendingDrawing(Tree &t);
static schema *generateAbstractionSchema(schema *x, Tree t);
static schema *generateOutputSlotSchema(Tree a);
static schema *generateInputSlotSchema(Tree a);
static schema *generateBargraphSchema(Tree t);
static schema *generateUserInterfaceSchema(Tree t);
static schema *generateSoundfileSchema(Tree t);
static char *legalFileName(Tree t, int n, char *dst);

static schema *addSchemaInputs(int ins, schema *x);
static schema *addSchemaOutputs(int outs, schema *x);

property<bool> *gPureRoutingProperty;
bool gFoldingFlag; // true with complex block-diagrams
int gFoldThreshold; // global complexity threshold before activating folding
int gFoldComplexity;  // individual complexity threshold before folding
set<Tree> gDrawnExp; // Expressions drawn or scheduled so far
map<Tree, string> gBackLink; // Link to enclosing file for sub schema
stack<Tree> gPendingExp; // Expressions that need to be drawn
string gSchemaFileName;  // Name of schema file being generated
Tree gInverter[6];

/**
 *The entry point to generate from a block diagram as a set of
 *svg files stored in the directory "<projname>-svg/" or
 *"<projname>-ps/" depending of <dev>.
 */
void drawBox(Box bd) {
    // Setup
    gPureRoutingProperty = new property<bool>();
    gInverter[0] = boxSeq(boxPar(boxWire(), boxInt(-1)), boxPrim2(sigMul));
    gInverter[1] = boxSeq(boxPar(boxInt(-1), boxWire()), boxPrim2(sigMul));
    gInverter[2] = boxSeq(boxPar(boxWire(), boxReal(-1.0)), boxPrim2(sigMul));
    gInverter[3] = boxSeq(boxPar(boxReal(-1.0), boxWire()), boxPrim2(sigMul));
    gInverter[4] = boxSeq(boxPar(boxInt(0), boxWire()), boxPrim2(sigSub));
    gInverter[5] = boxSeq(boxPar(boxReal(0.0), boxWire()), boxPrim2(sigSub));
    gFoldingFlag = boxComplexity(bd) > gFoldThreshold;
    gFoldThreshold = 25;
    gFoldComplexity = 2;

    mkchDir("FaustDiagrams"); // create a directory to store files

    scheduleDrawing(bd);    // schedule the initial drawing

    Tree t;
    while (pendingDrawing(t)) {
        writeSchemaFile(t); // generate all the pending drawing
    }

    choldDir();  // return to current directory
}

/************************************************************************
 ************************************************************************
                            IMPLEMENTATION
 ************************************************************************
 ************************************************************************/

// Collect the leaf numbers of tree l into vector v.
// return true if l a number or a parallel tree of numbers.
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

//------------------- to schedule and retrieve drawing ------------------

/**
 * Schedule a makeBlockSchema diagram to be drawn.
 */
static void scheduleDrawing(Tree t) {
    if (gDrawnExp.find(t) == gDrawnExp.end()) {
        gDrawnExp.insert(t);
        gBackLink.insert(make_pair(t, gSchemaFileName));  // remember the enclosing filename
        gPendingExp.push(t);
    }
}

/**
 * Retrieve next block diagram that must be drawn.
 */
static bool pendingDrawing(Tree &t) {
    if (gPendingExp.empty()) return false;
    t = gPendingExp.top();
    gPendingExp.pop();
    return true;
}

//------------------------ dealing with files -------------------------

/**
 * Write a top level diagram. A top level diagram
 * is decorated with its definition name property
 * and is drawn in an individual file.
 */
static void writeSchemaFile(Tree bd) {
    Tree id;
    schema *ts;
    int ins, outs;

    char temp[1024];

    getBoxType(bd, &ins, &outs);

    bool hasname = getDefNameProperty(bd, id);
    if (!hasname) id = tree(Node(unique("diagram_"))); // create an arbitrary name

    // generate legal file name for the schema
    stringstream s1;
    s1 << legalFileName(bd, 1024, temp) << ".svg";
    string res1 = s1.str();
    gSchemaFileName = res1;

    // generate the label of the schema
    string link = gBackLink[bd];
    ts = makeTopSchema(addSchemaOutputs(outs, addSchemaInputs(ins, generateInsideSchema(bd))), 20, tree2str(id), link);
    SVGDev dev(res1.c_str(), ts->width(), ts->height());
    ts->place(0, 0, kLeftRight);
    ts->draw(dev);
    {
        collector c;
        ts->collectTraits(c);
        c.draw(dev);
    }
}

/**
 * Transform the definition name property of tree <t> into a
 * legal file name.  The resulting file name is stored in
 * <dst> a table of at least <n> chars. Returns the <dst> pointer
 * for convenience.
 */
static char *legalFileName(Tree t, int n, char *dst) {
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

//------------------------ generating the schema -------------------------

/**
 * isInverter(t) returns true if t == '*(-1)'. This test is used
 * to simplify diagram by using a special symbol for inverters.
 */
static bool isInverter(Tree t) {
    for (const auto &i: gInverter) if (t == i) return true;
    return false;
}

/**
 * Compute the Pure Routing property, that is expressions
 * only made of cut, wires and slots. No labels will be
 * dispayed for pure routing expressions.
 */

static bool isPureRouting(Tree t) {
    bool r;
    int ID;
    Tree x, y;

    if (gPureRoutingProperty->get(t, r)) return r;

    if (isBoxCut(t) || isBoxWire(t) || isInverter(t) || isBoxSlot(t, &ID) ||
        (isBoxPar(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxSeq(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxSplit(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxMerge(t, x, y) && isPureRouting(x) && isPureRouting(y))) {
        gPureRoutingProperty->set(t, true);
        return true;
    }

    gPureRoutingProperty->set(t, false);
    return false;
}

/**
 * Generate an appropriate schema according to the type of block diagram.
 */
static schema *generateDiagramSchema(Tree t) {
    Tree id;
    int ins, outs;

    const bool hasname = getDefNameProperty(t, id);
    if (gFoldingFlag && boxComplexity(t) >= gFoldComplexity && hasname) {
        char temp[1024];
        getBoxType(t, &ins, &outs);
        stringstream l;
        l << legalFileName(t, 1024, temp) << ".svg";
        scheduleDrawing(t);
        return makeBlockSchema(ins, outs, tree2str(id), linkcolor, l.str());
    }
        // Not a slot, with a name. Draw a line around the object with its name.
    else if (hasname && !isPureRouting(t)) return makeDecorateSchema(generateInsideSchema(t), 10, tree2str(id));

    return generateInsideSchema(t); // normal case
}

/**
 * Generate the inside schema of a block diagram
 * according to its type.
 */
static schema *generateInsideSchema(Tree t) {
    Tree a, b, c, ff, l, type, name, file;
    int i;
    double r;
    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;

    if (getUserData(t) != nullptr) return makeBlockSchema(xtendedArity(t), 1, xtendedName(t), normalcolor, "");
    if (isInverter(t)) return makeInverterSchema(invcolor);

    if (isBoxInt(t, &i)) {
        stringstream s;
        s << i;
        return makeBlockSchema(0, 1, s.str(), numcolor, "");
    }
    if (isBoxReal(t, &r)) {
        stringstream s;
        s << r;
        return makeBlockSchema(0, 1, s.str(), numcolor, "");
    }
    if (isBoxWaveform(t)) return makeBlockSchema(0, 2, "waveform{...}", normalcolor, "");
    if (isBoxWire(t)) return makeCableSchema();
    if (isBoxCut(t)) return makeCutSchema();
    if (isBoxPrim0(t, &p0)) return makeBlockSchema(0, 1, prim0name(p0), normalcolor, "");
    if (isBoxPrim1(t, &p1)) return makeBlockSchema(1, 1, prim1name(p1), normalcolor, "");
    if (isBoxPrim2(t, &p2)) return makeBlockSchema(2, 1, prim2name(p2), normalcolor, "");
    if (isBoxPrim3(t, &p3)) return makeBlockSchema(3, 1, prim3name(p3), normalcolor, "");
    if (isBoxPrim4(t, &p4)) return makeBlockSchema(4, 1, prim4name(p4), normalcolor, "");
    if (isBoxPrim5(t, &p5)) return makeBlockSchema(5, 1, prim5name(p5), normalcolor, "");
    if (isBoxFFun(t, ff)) return makeBlockSchema(ffarity(ff), 1, ffname(ff), normalcolor, "");
    if (isBoxFConst(t, type, name, file)) return makeBlockSchema(0, 1, tree2str(name), normalcolor, "");
    if (isBoxFVar(t, type, name, file)) return makeBlockSchema(0, 1, tree2str(name), normalcolor, "");
    if (isBoxButton(t)) return generateUserInterfaceSchema(t);
    if (isBoxCheckbox(t)) return generateUserInterfaceSchema(t);
    if (isBoxVSlider(t)) return generateUserInterfaceSchema(t);
    if (isBoxHSlider(t)) return generateUserInterfaceSchema(t);
    if (isBoxNumEntry(t)) return generateUserInterfaceSchema(t);
    if (isBoxVBargraph(t)) return generateBargraphSchema(t);
    if (isBoxHBargraph(t)) return generateBargraphSchema(t);
    if (isBoxSoundfile(t)) return generateSoundfileSchema(t);
    if (isBoxMetadata(t, a, b)) return generateDiagramSchema(a);

    // Don't draw group rectangle when labels are empty (ie "")
    if (isBoxVGroup(t, l, a)) {
        schema *s1 = generateDiagramSchema(a);
        return makeDecorateSchema(s1, 10, "vgroup(" + extractName(l) + ")");
    }
    if (isBoxHGroup(t, l, a)) {
        schema *s1 = generateDiagramSchema(a);
        return makeDecorateSchema(s1, 10, "hgroup(" + extractName(l) + ")");
    }
    if (isBoxTGroup(t, l, a)) {
        schema *s1 = generateDiagramSchema(a);
        return makeDecorateSchema(s1, 10, "tgroup(" + extractName(l) + ")");
    }
    if (isBoxSeq(t, a, b)) return makeSeqSchema(generateDiagramSchema(a), generateDiagramSchema(b));
    if (isBoxPar(t, a, b)) return makeParSchema(generateDiagramSchema(a), generateDiagramSchema(b));
    if (isBoxSplit(t, a, b)) return makeSplitSchema(generateDiagramSchema(a), generateDiagramSchema(b));
    if (isBoxMerge(t, a, b)) return makeMergeSchema(generateDiagramSchema(a), generateDiagramSchema(b));
    if (isBoxRec(t, a, b)) return makeRecSchema(generateDiagramSchema(a), generateDiagramSchema(b));
    if (isBoxSlot(t, &i)) return generateOutputSlotSchema(t);
    if (isBoxSymbolic(t, a, b)) {
        Tree id;
        if (getDefNameProperty(t, id)) return generateAbstractionSchema(generateInputSlotSchema(a), b);
        return makeDecorateSchema(generateAbstractionSchema(generateInputSlotSchema(a), b), 10, "Abstraction");
    }
    if (isBoxEnvironment(t)) return makeBlockSchema(0, 0, "environment{...}", normalcolor, "");
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

/**
 * Convert User interface element into a textual representation
 */
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

/**
 * Generate a 0->1 block schema for a user interface element.
 */
static schema *generateUserInterfaceSchema(Tree t) {
    return makeBlockSchema(0, 1, userInterfaceDescription(t), uicolor, "");
}

/**
 * Generate a 1->1 block schema for a user interface bargraph.
 */
static schema *generateBargraphSchema(Tree t) {
    return makeBlockSchema(1, 1, userInterfaceDescription(t), uicolor, "");
}

/**
 * Generate a 2->3+c block schema for soundfile("toto",c).
 */
static schema *generateSoundfileSchema(Tree t) {
    Tree label, chan;
    if (isBoxSoundfile(t, label, chan)) {
        int n = tree2int(chan);
        return makeBlockSchema(2, 2 + n, userInterfaceDescription(t), uicolor, "");
    }
    throw std::runtime_error("ERROR : soundfile");
}

/**
 * Generate a 1->0 block schema for an input slot.
 */
static schema *generateInputSlotSchema(Tree a) {
    Tree id;
    getDefNameProperty(a, id);
    return makeBlockSchema(1, 0, tree2str(id), slotcolor, "");
}

/**
 * Generate a 0->1 block schema for an output slot.
 */
static schema *generateOutputSlotSchema(Tree a) {
    Tree id;
    getDefNameProperty(a, id);
    return makeBlockSchema(0, 1, tree2str(id), slotcolor, "");
}

/**
 * Generate an abstraction schema by placing in sequence
 * the input slots and the body.
 */
static schema *generateAbstractionSchema(schema *x, Tree t) {
    Tree a, b;

    while (isBoxSymbolic(t, a, b)) {
        x = makeParSchema(x, generateInputSlotSchema(a));
        t = b;
    }
    return makeSeqSchema(x, generateDiagramSchema(t));
}

static schema *addSchemaInputs(int ins, schema *x) {
    if (ins == 0) return x;

    schema *y = nullptr;
    do {
        schema *z = makeConnectorSchema();
        y = y != nullptr ? makeParSchema(y, z) : z;
    } while (--ins);

    return makeSeqSchema(y, x);
}

static schema *addSchemaOutputs(int outs, schema *x) {
    if (outs == 0) return x;

    schema *y = nullptr;
    do {
        schema *z = makeConnectorSchema();
        y = y != nullptr ? makeParSchema(y, z) : z;
    } while (--outs);

    return makeSeqSchema(x, y);
}
