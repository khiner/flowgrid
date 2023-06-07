#include "FaustNode.h"

#include "App/Audio/Sample.h" // Must be included before any Faust includes.
#include "faust/dsp/llvm-dsp.h"

#include "miniaudio.h"

#include "App/Audio/AudioDevice.h"
#include "Core/Stateful/FieldAction.h"
#include "Faust.h"
#include "FaustBox.h"
#include "FaustParams.h"

namespace FaustContext {
static dsp *Dsp = nullptr;
static std::unique_ptr<FaustParams> Ui;

static void Init() {
    if (Dsp || !faust.IsReady()) return;

    createLibContext();

    const char *libraries_path = fs::relative("../lib/faust/libraries").c_str();
    vector<const char *> argv = {"-I", libraries_path};
    if (std::is_same_v<Sample, double>) argv.push_back("-double");

    const int argc = argv.size();
    static int num_inputs, num_outputs;
    static string error_msg;
    const Box box = DSPToBoxes("FlowGrid", faust.Code, argc, argv.data(), &num_inputs, &num_outputs, error_msg);

    static llvm_dsp_factory *dsp_factory;
    if (box && error_msg.empty()) {
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", box, argc, argv.data(), "", error_msg, optimize_level);
    }
    if (!box && error_msg.empty()) error_msg = "`DSPToBoxes` returned no error but did not produce a result.";

    if (dsp_factory && error_msg.empty()) {
        Dsp = dsp_factory->createDSPInstance();
        if (!Dsp) error_msg = "Could not create Faust DSP.";
        else {
            Ui = std::make_unique<FaustParams>();
            Dsp->buildUserInterface(Ui.get());
            // `Dsp->Init` happens in the Faust graph node.
        }
    }

    const auto &ErrorLog = faust.Log.Error;
    if (!error_msg.empty()) Action::SetValue{ErrorLog.Path, error_msg}.q();
    else if (ErrorLog) Action::SetValue{ErrorLog.Path, ""}.q();

    OnBoxChange(box);
    OnUiChange(Ui.get());
}

static void Uninit() {
    OnBoxChange(nullptr);
    OnUiChange(nullptr);

    Ui = nullptr;
    if (Dsp) {
        delete Dsp;
        Dsp = nullptr;
        deleteAllDSPFactories(); // There should only be one factory, but using this instead of `deleteDSPFactory` avoids storing another file-scoped variable.
    }

    destroyLibContext();
}

static void Update() {
    const bool ready = faust.IsReady();
    const bool needs_restart = faust.NeedsRestart(); // Don't inline! Must run during every update.
    if (!Dsp && ready) {
        Init();
    } else if (Dsp && !ready) {
        Uninit();
    } else if (needs_restart) {
        Uninit();
        Init();
    }
}
} // namespace FaustContext

void FaustProcess(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **bus_frames_in = const_cast<float **>(const_bus_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    if (FaustContext::Dsp) FaustContext::Dsp->compute(*frame_count_out, bus_frames_in, bus_frames_out);

    (void)node; // unused
    (void)frame_count_in; // unused
}

void FaustNode::DoInit(ma_node_graph *graph) const {
    if (FaustContext::Dsp) throw std::runtime_error("Faust DSP already initialized.");

    FaustContext::Init();
    FaustContext::Dsp->init(audio_device.SampleRate);
    const Count in_channels = FaustContext::Dsp->getNumInputs();
    const Count out_channels = FaustContext::Dsp->getNumOutputs();
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

void FaustNode::DoUninit() const {
    FaustContext::Uninit();
}

void FaustNode::DoUpdate() const {
    FaustContext::Update();
}

bool FaustNode::NeedsRestart() const {
    static dsp *PreviousDsp = FaustContext::Dsp;
    static U32 PreviousSampleRate = audio_device.SampleRate;

    const bool needs_restart = FaustContext::Dsp != PreviousDsp || audio_device.SampleRate != PreviousSampleRate;
    PreviousDsp = FaustContext::Dsp;
    PreviousSampleRate = audio_device.SampleRate;

    return needs_restart;
}
