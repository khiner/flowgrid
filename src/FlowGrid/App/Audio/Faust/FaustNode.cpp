#include "FaustNode.h"

#include "App/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

#include "miniaudio.h"

#include "App/Audio/AudioDevice.h"
#include "Faust.h"
#include "FaustBox.h"
#include "FaustDspListener.h"

static dsp *CurrentDsp; // Only used in `FaustProcess`. todo pass in `ma_node` userdata instead?
static Box box;

FaustNode::FaustNode(ComponentArgs &&args, bool on) : AudioGraphNode(std::move(args), on) {
    Field::RegisterChangeListener(audio_device.SampleRate, this);
}
FaustNode::~FaustNode() {
    Field::UnregisterChangeListener(this);
}

void FaustNode::InitDsp() {
    if (Dsp || !faust.Code) return;

    createLibContext();

    const char *libraries_path = fs::relative("../lib/faust/libraries").c_str();
    std::vector<const char *> argv = {"-I", libraries_path};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");

    const int argc = argv.size();
    static int num_inputs, num_outputs;
    string &error_message = faust.Log.ErrorMessage;
    box = DSPToBoxes("FlowGrid", faust.Code, argc, argv.data(), &num_inputs, &num_outputs, error_message);
    OnBoxChange(box);

    static llvm_dsp_factory *dsp_factory;
    if (box && error_message.empty()) {
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", box, argc, argv.data(), "", error_message, optimize_level);
        if (dsp_factory && error_message.empty()) {
            Dsp = dsp_factory->createDSPInstance();
            if (!Dsp) error_message = "Could not create Faust DSP.";
        }
    } else if (!box && error_message.empty()) {
        error_message = "`DSPToBoxes` returned no error but did not produce a result.";
    }

    OnFaustDspChange(Dsp);
}

void FaustNode::UninitDsp() {
    OnBoxChange(nullptr);
    OnFaustDspChange(nullptr);

    CurrentDsp = nullptr;
    if (Dsp) {
        delete Dsp;
        Dsp = nullptr;
        deleteAllDSPFactories(); // There should only be one factory, but using this instead of `deleteDSPFactory` avoids storing another file-scoped variable.
    }

    destroyLibContext();
}

void FaustProcess(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **bus_frames_in = const_cast<float **>(const_bus_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    if (CurrentDsp) CurrentDsp->compute(*frame_count_out, bus_frames_in, bus_frames_out);

    (void)node; // unused
    (void)frame_count_in; // unused
}

void FaustNode::DoInit(ma_node_graph *graph) {
    if (!Dsp) return;

    Dsp->init(audio_device.SampleRate);
    CurrentDsp = Dsp;

    const Count in_channels = Dsp->getNumInputs();
    const Count out_channels = Dsp->getNumOutputs();
    if (in_channels == 0 && out_channels == 0) return;

    static ma_node_vtable vtable{};
    vtable = {FaustProcess, nullptr, ma_uint8(in_channels > 0 ? 1 : 0), ma_uint8(out_channels > 0 ? 1 : 0), 0};

    static ma_node_config config;
    config = ma_node_config_init();

    config.pInputChannels = &in_channels; // One input bus with N channels.
    config.pOutputChannels = &out_channels; // One output bus with M channels.
    config.vtable = &vtable;

    static ma_node_base node{};
    const int result = ma_node_init(graph, &config, nullptr, &node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust node: {}", result));

    Set(&node);
}
