// #include "CDSPResampler.h"
#include "Sample.h" // Must be included before any Faust includes
#include "faust/dsp/llvm-dsp.h"

#include <range/v3/core.hpp>
#include <range/v3/view/transform.hpp>

#include "../Helper/File.h"
#include "../Helper/String.h"
#include "Audio.h"
#include "Faust/FaustGraph.h"
#include "Faust/FaustParams.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "../UI/Widgets.h"

using namespace ImGui;

string to_string(const IO io, const bool shorten) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
        default: return "unknown";
    }
}

ImGuiTableFlags TableFlagsToImGui(const TableFlags flags) {
    ImGuiTableFlags imgui_flags = ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingStretchProp;
    if (flags & TableFlags_Resizable) imgui_flags |= ImGuiTableFlags_Resizable;
    if (flags & TableFlags_Reorderable) imgui_flags |= ImGuiTableFlags_Reorderable;
    if (flags & TableFlags_Hideable) imgui_flags |= ImGuiTableFlags_Hideable;
    if (flags & TableFlags_Sortable) imgui_flags |= ImGuiTableFlags_Sortable;
    if (flags & TableFlags_ContextMenuInBody) imgui_flags |= ImGuiTableFlags_ContextMenuInBody;
    if (flags & TableFlags_BordersInnerH) imgui_flags |= ImGuiTableFlags_BordersInnerH;
    if (flags & TableFlags_BordersOuterH) imgui_flags |= ImGuiTableFlags_BordersOuterH;
    if (flags & TableFlags_BordersInnerV) imgui_flags |= ImGuiTableFlags_BordersInnerV;
    if (flags & TableFlags_BordersOuterV) imgui_flags |= ImGuiTableFlags_BordersOuterV;
    if (flags & TableFlags_NoBordersInBody) imgui_flags |= ImGuiTableFlags_NoBordersInBody;
    if (flags & TableFlags_PadOuterX) imgui_flags |= ImGuiTableFlags_PadOuterX;
    if (flags & TableFlags_NoPadOuterX) imgui_flags |= ImGuiTableFlags_NoPadOuterX;
    if (flags & TableFlags_NoPadInnerX) imgui_flags |= ImGuiTableFlags_NoPadInnerX;

    return imgui_flags;
}

// Graph style

Audio::Faust::FaustGraph::Style::Style(StateMember *parent, string_view path_segment, string_view name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark();
    LayoutFlowGrid();
}

const char *Audio::Faust::FaustGraph::Style::GetColorName(FlowGridGraphCol idx) {
    switch (idx) {
        case FlowGridGraphCol_Bg: return "Background";
        case FlowGridGraphCol_Text: return "Text";
        case FlowGridGraphCol_DecorateStroke: return "DecorateStroke";
        case FlowGridGraphCol_GroupStroke: return "GroupStroke";
        case FlowGridGraphCol_Line: return "Line";
        case FlowGridGraphCol_Link: return "Link";
        case FlowGridGraphCol_Inverter: return "Inverter";
        case FlowGridGraphCol_OrientationMark: return "OrientationMark";
        case FlowGridGraphCol_Normal: return "Normal";
        case FlowGridGraphCol_Ui: return "Ui";
        case FlowGridGraphCol_Slot: return "Slot";
        case FlowGridGraphCol_Number: return "Number";
        default: return "Unknown";
    }
}

void Audio::Faust::FaustGraph::Style::ColorsDark() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0.06, 0.06, 0.06, 0.94}},
            {FlowGridGraphCol_Text, {1, 1, 1, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridGraphCol_GroupStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridGraphCol_Line, {0.61, 0.61, 0.61, 1}},
            {FlowGridGraphCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridGraphCol_Inverter, {1, 1, 1, 1}},
            {FlowGridGraphCol_OrientationMark, {1, 1, 1, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}
void Audio::Faust::FaustGraph::Style::ColorsClassic() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0, 0, 0, 0.85}},
            {FlowGridGraphCol_Text, {0.9, 0.9, 0.9, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridGraphCol_GroupStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridGraphCol_Line, {1, 1, 1, 1}},
            {FlowGridGraphCol_Link, {0.35, 0.4, 0.61, 0.62}},
            {FlowGridGraphCol_Inverter, {0.9, 0.9, 0.9, 1}},
            {FlowGridGraphCol_OrientationMark, {0.9, 0.9, 0.9, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}
void Audio::Faust::FaustGraph::Style::ColorsLight() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0.94, 0.94, 0.94, 1}},
            {FlowGridGraphCol_Text, {0, 0, 0, 1}},
            {FlowGridGraphCol_DecorateStroke, {0, 0, 0, 0.3}},
            {FlowGridGraphCol_GroupStroke, {0, 0, 0, 0.3}},
            {FlowGridGraphCol_Line, {0.39, 0.39, 0.39, 1}},
            {FlowGridGraphCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridGraphCol_Inverter, {0, 0, 0, 1}},
            {FlowGridGraphCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}
void Audio::Faust::FaustGraph::Style::ColorsFaust() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {1, 1, 1, 1}},
            {FlowGridGraphCol_Text, {1, 1, 1, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridGraphCol_GroupStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridGraphCol_Line, {0, 0, 0, 1}},
            {FlowGridGraphCol_Link, {0, 0.2, 0.4, 1}},
            {FlowGridGraphCol_Inverter, {0, 0, 0, 1}},
            {FlowGridGraphCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}

void Audio::Faust::FaustGraph::Style::LayoutFlowGrid() const {
    static const auto DefaultLayoutEntries = LayoutFields | ranges::views::transform([](const PrimitiveBase &field) { return Field::Entry(field, field.Get()); }) | ranges::to<const Field::Entries>;
    store::Set(DefaultLayoutEntries);
}
void Audio::Faust::FaustGraph::Style::LayoutFaust() const {
    store::Set(
        {
            {SequentialConnectionZigzag, true},
            {OrientationMark, true},
            {DecorateRootNode, true},
            {DecorateMargin.X, 10},
            {DecorateMargin.Y, 10},
            {DecoratePadding.X, 10},
            {DecoratePadding.Y, 10},
            {DecorateLineWidth, 1},
            {DecorateCornerRadius, 0},
            {GroupMargin.X, 10},
            {GroupMargin.Y, 10},
            {GroupPadding.X, 10},
            {GroupPadding.Y, 10},
            {GroupLineWidth, 1},
            {GroupCornerRadius, 0},
            {BoxCornerRadius, 0},
            {BinaryHorizontalGapRatio, 0.25f},
            {WireWidth, 1},
            {WireGap, 16},
            {NodeMargin.X, 8},
            {NodeMargin.Y, 8},
            {NodePadding.X, 8},
            {NodePadding.Y, 0},
            {ArrowSize.X, 3},
            {ArrowSize.Y, 2},
            {InverterRadius, 3},
        }
    );
}

void Audio::Faust::FaustGraph::Style::Render() const {
    if (BeginTabBar(ImGuiLabel.c_str(), ImGuiTabBarFlags_None)) {
        if (BeginTabItem("Layout")) {
            static int graph_layout_idx = -1;
            if (Combo("Preset", &graph_layout_idx, "FlowGrid\0Faust\0")) q(Action::SetGraphLayoutStyle{graph_layout_idx});

            FoldComplexity.Draw();
            const bool scale_fill = ScaleFillHeight;
            ScaleFillHeight.Draw();
            if (scale_fill) BeginDisabled();
            Scale.Draw();
            if (scale_fill) {
                SameLine();
                TextUnformatted(std::format("Uncheck '{}' to manually edit graph scale.", ScaleFillHeight.Name).c_str());
                EndDisabled();
            }
            Direction.Draw();
            OrientationMark.Draw();
            if (OrientationMark) {
                SameLine();
                SetNextItemWidth(GetContentRegionAvail().x * 0.5f);
                OrientationMarkRadius.Draw();
            }
            RouteFrame.Draw();
            SequentialConnectionZigzag.Draw();
            Separator();
            const bool decorate_folded = DecorateRootNode;
            DecorateRootNode.Draw();
            if (!decorate_folded) BeginDisabled();
            DecorateMargin.Draw();
            DecoratePadding.Draw();
            DecorateLineWidth.Draw();
            DecorateCornerRadius.Draw();
            if (!decorate_folded) EndDisabled();
            Separator();
            GroupMargin.Draw();
            GroupPadding.Draw();
            GroupLineWidth.Draw();
            GroupCornerRadius.Draw();
            Separator();
            NodeMargin.Draw();
            NodePadding.Draw();
            BoxCornerRadius.Draw();
            BinaryHorizontalGapRatio.Draw();
            WireGap.Draw();
            WireWidth.Draw();
            ArrowSize.Draw();
            InverterRadius.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Colors.ImGuiLabel.c_str())) {
            static int graph_colors_idx = -1;
            if (Combo("Preset", &graph_colors_idx, "Dark\0Light\0Classic\0Faust\0")) q(Action::SetGraphColorStyle{graph_colors_idx});

            Colors.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}

void Audio::Faust::FaustParams::Style::Render() const {
    HeaderTitles.Draw();
    MinHorizontalItemWidth.Draw();
    MaxHorizontalItemWidth.Draw();
    MinVerticalItemHeight.Draw();
    MinKnobItemSize.Draw();
    AlignmentHorizontal.Draw();
    AlignmentVertical.Draw();
    Spacing();
    WidthSizingPolicy.Draw();
    TableFlags.Draw();
}

void Audio::Graph::Style::Matrix::Render() const {
    CellSize.Draw();
    CellGap.Draw();
    LabelSize.Draw();
}

// todo implement for r8brain resampler
// todo I want to use this currently to support quality/fast resampling between _natively supported_ device sample rates.
//   Can I still use duplex mode in this case?
// #include "CDSPResampler.h"
// See https://github.com/avaneev/r8brain-free-src/issues/12 for resampling latency calculation
// static unique_ptr<r8b::CDSPResampler24> Resampler;
// int resampled_frames = Resampler->process(read_ptr, available_resample_read_frames, resampled_buffer);
// Set up resampler if needed.
// if (InStream->sample_rate != OutStream->sample_rate) {
// Resampler = make_unique<r8b::CDSPResampler24>(InStream->sample_rate, OutStream->sample_rate, 1024); // todo can we get max frame size here?
// }
// static ma_resampling_backend_vtable ResamplerVTable = {
//     ma_resampling_backend_get_heap_size__linear,
//     ma_resampling_backend_init__linear,
//     ma_resampling_backend_uninit__linear,
//     ma_resampling_backend_process__linear,
//     ma_resampling_backend_set_rate__linear,
//     ma_resampling_backend_get_input_latency__linear,
//     ma_resampling_backend_get_output_latency__linear,
//     ma_resampling_backend_get_required_input_frame_count__linear,
//     ma_resampling_backend_get_expected_output_frame_count__linear,
//     ma_resampling_backend_reset__linear,
// };

static ma_context AudioContext;
static vector<ma_device_info *> DeviceInfos[IO_Count];
static vector<string> DeviceNames[IO_Count];
static const ma_device_id *GetDeviceId(IO io, string_view device_name) {
    for (const ma_device_info *info : DeviceInfos[io]) {
        if (info->name == device_name) return &(info->id);
    }
    return nullptr;
}

// static ma_resampler_config ResamplerConfig;
// static ma_resampler Resampler;

// todo explicit re-scan action.
void Audio::Init() const {
    for (const IO io : IO_All) {
        DeviceInfos[io].clear();
        DeviceNames[io].clear();
    }

    int result = ma_context_init(nullptr, 0, nullptr, &AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio context: {}", result));

    static Count PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio devices: {}", result));

    for (Count i = 0; i < CaptureDeviceCount; i++) {
        DeviceInfos[IO_In].emplace_back(&CaptureDeviceInfos[i]);
        DeviceNames[IO_In].push_back(CaptureDeviceInfos[i].name);
    }
    for (Count i = 0; i < PlaybackDeviceCount; i++) {
        DeviceInfos[IO_Out].emplace_back(&PlaybackDeviceInfos[i]);
        DeviceNames[IO_Out].push_back(PlaybackDeviceInfos[i].name);
    }

    Device.Init();
    Graph.Init();
    Device.Start();

    NeedsRestart(); // xxx Updates cached values as side effect.
}

void Audio::Uninit() const {
    Device.Stop();
    Graph.Uninit();
    Device.Uninit();

    const int result = ma_context_uninit(&AudioContext);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error shutting down audio context: {}", result));
}

// todo draw debug info for all devices, not just current
//  void DrawDevices() {
//      for (const IO io : IO_All) {
//          const Count device_count = GetDeviceCount(io);
//          if (TreeNodeEx(std::format("{} devices ({})", Capitalize(to_string(io)), device_count).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
//              for (Count device_index = 0; device_index < device_count; device_index++) {
//                  auto *device = GetDevice(io, device_index);
//                  ShowDevice(*device);
//              }
//              TreePop();
//          }
//      }
//  }

// void ShowBufferPlots() {
//     for (IO io : IO_All) {
//         const bool is_in = io == IO_In;
//         if (TreeNode(Capitalize(to_string(io)).c_str())) {
//             const auto *area = is_in ? Areas[IO_In] : Areas[IO_Out];
//             if (!area) continue;

//             const auto *device = is_in ? InStream->device : OutStream->device;
//             const auto &layout = is_in ? InStream->layout : OutStream->layout;
//             const auto frame_count = is_in ? LastReadFrameCount : LastWriteFrameCount;
//             if (ImPlot::BeginPlot(device->name, {-1, 160})) {
//                 ImPlot::SetupAxes("Sample index", "Value");
//                 ImPlot::SetupAxisLimits(ImAxis_X1, 0, frame_count, ImGuiCond_Always);
//                 ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
//                 for (int channel_index = 0; channel_index < layout.channel_count; channel_index++) {
//                     const auto &channel = layout.channels[channel_index];
//                     const char *channel_name = soundio_get_channel_name(channel);
//                     // todo Adapt the pointer casting to the sample format.
//                     //  Also, this works but very scary and I can't even justify why this seems to work so well,
//                     //  since the area pointer position gets updated in the separate read/write callbacks.
//                     //  Hrm.. are the start points of each channel area static after initializing the stream?
//                     //  If so, could just set those once on stream init and use them here!
//                     ImPlot::PlotLine(channel_name, (Sample *)area[channel_index].ptr, frame_count);
//                 }
//                 ImPlot::EndPlot();
//             }
//             TreePop();
//         }
//     }
// }

void Audio::Render() const {
    Update();
    TabsWindow::Render({Faust.Id}); // Exclude the Faust tab.
}

namespace FaustContext {
static dsp *Dsp = nullptr;
static std::unique_ptr<FaustParams> Ui;

static void Init() {
    createLibContext();

    const string libraries_path = fs::relative("../lib/faust/libraries").string();
    vector<const char *> argv;
    argv.reserve(8);
    argv.push_back("-I");
    argv.push_back(libraries_path.c_str());
    if (std::is_same_v<Sample, double>) argv.push_back("-double");

    const int argc = argv.size();
    static int num_inputs, num_outputs;
    static string error_msg;
    const Box box = DSPToBoxes("FlowGrid", audio.Faust.Code, argc, argv.data(), &num_inputs, &num_outputs, error_msg);

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

    const auto &ErrorLog = audio.Faust.Log.Error;
    if (!error_msg.empty()) q(Action::SetValue{ErrorLog.Path, error_msg});
    else if (ErrorLog) q(Action::SetValue{ErrorLog.Path, ""});

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

static bool NeedsRestart() {
    static string PreviousFaustCode = audio.Faust.Code;

    const bool needs_restart = audio.Faust.Code != PreviousFaustCode;
    PreviousFaustCode = audio.Faust.Code;
    return needs_restart;
}
} // namespace FaustContext

void Audio::Update() const {
    // Faust setup is only dependent on the faust code.
    // TODO now that we're not dependent on global App.h state, we need to find a way to destroy the Faust context when the app closes.
    // const bool is_faust_initialized = s.UiProcess.Running && Faust.Code && !Faust.Log.Error;
    const bool is_faust_initialized = Faust.Code && !Faust.Log.Error;
    const bool faust_needs_restart = FaustContext::NeedsRestart(); // Don't inline! Must run during every update.
    if (!FaustContext::Dsp && is_faust_initialized) {
        FaustContext::Init();
    } else if (FaustContext::Dsp && !is_faust_initialized) {
        FaustContext::Uninit();
    } else if (faust_needs_restart) {
        FaustContext::Uninit();
        FaustContext::Init();
    }

    const bool is_initialized = Device.IsStarted();
    const bool needs_restart = NeedsRestart(); // Don't inline! Must run during every update.
    if (Device.On && !is_initialized) {
        Init();
    } else if (!Device.On && is_initialized) {
        Uninit();
    } else if (needs_restart && is_initialized) {
        // todo no need to completely reset in many cases (like when only format has changed) - just modify as needed in `Device::Update`.
        // todo sample rate conversion is happening even when choosing a SR that is native to both intpu & output, if it's not the highest-priority SR.
        Uninit();
        Init();
    }

    Device.Update();

    if (Device.IsStarted()) Graph.Update();
}

bool Audio::NeedsRestart() const {
    static string PreviousInDeviceName = Device.InDeviceName, PreviousOutDeviceName = Device.OutDeviceName;
    static int PreviousInFormat = Device.InFormat, PreviousOutFormat = Device.OutFormat;
    static U32 PreviousSampleRate = Device.SampleRate;

    const bool needs_restart =
        PreviousInDeviceName != Device.InDeviceName ||
        PreviousOutDeviceName != Device.OutDeviceName ||
        PreviousInFormat != Device.InFormat || PreviousOutFormat != Device.OutFormat ||
        PreviousSampleRate != Device.SampleRate;

    PreviousInDeviceName = Device.InDeviceName;
    PreviousOutDeviceName = Device.OutDeviceName;
    PreviousInFormat = Device.InFormat;
    PreviousOutFormat = Device.OutFormat;
    PreviousSampleRate = Device.SampleRate;

    return needs_restart;
}

// todo support loopback mode? (think of use cases)

const vector<U32> Audio::Device::PrioritizedSampleRates = {std::begin(g_maStandardSampleRatePriorities), std::end(g_maStandardSampleRatePriorities)};

static vector<ma_format> NativeFormats;
static vector<U32> NativeSampleRates;

const string Audio::Device::GetFormatName(const int format) {
    const bool is_native = std::find(NativeFormats.begin(), NativeFormats.end(), format) != NativeFormats.end();
    return ::std::format("{}{}", ma_get_format_name((ma_format)format), is_native ? "*" : "");
}
const string Audio::Device::GetSampleRateName(const U32 sample_rate) {
    const bool is_native = std::find(NativeSampleRates.begin(), NativeSampleRates.end(), sample_rate) != NativeSampleRates.end();
    return std::format("{}{}", to_string(sample_rate), is_native ? "*" : "");
}

// Current device
static ma_device MaDevice;
static ma_device_config DeviceConfig;
static ma_device_info DeviceInfo;

static ma_node_graph NodeGraph;
static ma_node_graph_config NodeGraphConfig;
static ma_audio_buffer_ref InputBuffer;

void DataCallback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    ma_audio_buffer_ref_set_data(&InputBuffer, input, frame_count);
    ma_node_graph_read_pcm_frames(&NodeGraph, output, frame_count, nullptr);
    (void)device; // unused
}

void Audio::Device::Init() const {
    DeviceConfig = ma_device_config_init(ma_device_type_duplex);
    DeviceConfig.capture.pDeviceID = GetDeviceId(IO_In, InDeviceName);
    DeviceConfig.capture.format = ma_format_f32;
    DeviceConfig.capture.channels = 1; // Temporary (2)
    DeviceConfig.capture.shareMode = ma_share_mode_shared;
    DeviceConfig.playback.pDeviceID = GetDeviceId(IO_Out, OutDeviceName);
    DeviceConfig.playback.format = ma_format_f32;
    DeviceConfig.playback.channels = 1; // Temporary (2)
    DeviceConfig.dataCallback = DataCallback;
    DeviceConfig.sampleRate = SampleRate;

    // MA graph nodes require f32 format for in/out.
    // We could keep IO formats configurable, and add two decoders to/from f32, but MA already does this
    // conversion from native formats (if needed) since we specify f32 format in the device config, so it
    // would just be needlessly wasting cycles/memory (memory since an extra input buffer would be needed).
    // todo option to change dither mode, only present when used
    // DeviceConfig.capture.format = ToAudioFormat(InFormat);
    // DeviceConfig.playback.format = ToAudioFormat(OutFormat);

    // ResamplerConfig = ma_resampler_config_init(ma_format_f32, 2, 0, 0, ma_resample_algorithm_custom);
    // auto result = ma_resampler_init(&ResamplerConfig, nullptr, &Resampler);
    // if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;

    int result = ma_device_init(nullptr, &DeviceConfig, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(MaDevice.pContext, MaDevice.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error getting audio device info: {}", result));

    // todo need to clarify that the cross-product of these formats & sample rates are supported natively, and not just each config jointly
    for (Count i = 0; i < DeviceInfo.nativeDataFormatCount; i++) {
        const auto &native_format = DeviceInfo.nativeDataFormats[i];
        NativeFormats.emplace_back(native_format.format);
        NativeSampleRates.emplace_back(native_format.sampleRate);
    }

    StoreEntries initial_settings;
    if (MaDevice.capture.name != InDeviceName) initial_settings.emplace_back(InDeviceName.Path, MaDevice.capture.name);
    if (MaDevice.playback.name != OutDeviceName) initial_settings.emplace_back(OutDeviceName.Path, MaDevice.playback.name);
    if (MaDevice.capture.format != InFormat) initial_settings.emplace_back(InFormat.Path, MaDevice.capture.format);
    if (MaDevice.playback.format != OutFormat) initial_settings.emplace_back(OutFormat.Path, MaDevice.playback.format);
    if (MaDevice.sampleRate != SampleRate) initial_settings.emplace_back(SampleRate.Path, MaDevice.sampleRate);
    if (!initial_settings.empty()) q(Action::SetValues{initial_settings}, true);
}

void Audio::Device::Update() const {
    if (IsStarted()) ma_device_set_master_volume(&MaDevice, Volume);
}

void Audio::Device::Render() const {
    On.Draw();
    if (!IsStarted()) {
        TextUnformatted("No audio device started yet");
        return;
    }
    Muted.Draw();
    SameLine();
    Volume.Draw();
    SampleRate.Render(PrioritizedSampleRates);
    for (const IO io : IO_All) {
        TextUnformatted(StringHelper::Capitalize(to_string(io)).c_str());
        (io == IO_In ? InDeviceName : OutDeviceName).Render(DeviceNames[io]);
        // (io == IO_In ? InFormat : OutFormat).Render(PrioritizedFormats); // See above - always using f32 format.
    }
    if (TreeNode("Info")) {
        auto *device = &MaDevice;
        assert(device->type == ma_device_type_duplex || device->type == ma_device_type_loopback);

        Text("[%s]", ma_get_backend_name(device->pContext->backend));

        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        ma_device_get_name(device, device->type == ma_device_type_loopback ? ma_device_type_playback : ma_device_type_capture, name, sizeof(name), nullptr);
        if (TreeNode(std::format("{} ({})", name, "Capture").c_str())) {
            Text("Format: %s -> %s", ma_get_format_name(device->capture.internalFormat), ma_get_format_name(device->capture.format));
            Text("Channels: %d -> %d", device->capture.internalChannels, device->capture.channels);
            Text("Sample Rate: %d -> %d", device->capture.internalSampleRate, device->sampleRate);
            Text("Buffer Size: %d*%d (%d)\n", device->capture.internalPeriodSizeInFrames, device->capture.internalPeriods, (device->capture.internalPeriodSizeInFrames * device->capture.internalPeriods));
            if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen)) {
                Text("Pre Format Conversion: %s\n", device->capture.converter.hasPreFormatConversion ? "YES" : "NO");
                Text("Post Format Conversion: %s\n", device->capture.converter.hasPostFormatConversion ? "YES" : "NO");
                Text("Channel Routing: %s\n", device->capture.converter.hasChannelConverter ? "YES" : "NO");
                Text("Resampling: %s\n", device->capture.converter.hasResampler ? "YES" : "NO");
                Text("Passthrough: %s\n", device->capture.converter.isPassthrough ? "YES" : "NO");
                {
                    char channel_map[1024];
                    ma_channel_map_to_string(device->capture.internalChannelMap, device->capture.internalChannels, channel_map, sizeof(channel_map));
                    Text("Channel Map In: {%s}\n", channel_map);

                    ma_channel_map_to_string(device->capture.channelMap, device->capture.channels, channel_map, sizeof(channel_map));
                    Text("Channel Map Out: {%s}\n", channel_map);
                }
                TreePop();
            }
            TreePop();
        }

        if (device->type == ma_device_type_loopback) return;

        ma_device_get_name(device, ma_device_type_playback, name, sizeof(name), nullptr);
        if (TreeNode(std::format("{} ({})", name, "Playback").c_str())) {
            Text("Format: %s -> %s", ma_get_format_name(device->playback.format), ma_get_format_name(device->playback.internalFormat));
            Text("Channels: %d -> %d", device->playback.channels, device->playback.internalChannels);
            Text("Sample Rate: %d -> %d", device->sampleRate, device->playback.internalSampleRate);
            Text("Buffer Size: %d*%d (%d)", device->playback.internalPeriodSizeInFrames, device->playback.internalPeriods, (device->playback.internalPeriodSizeInFrames * device->playback.internalPeriods));
            if (TreeNodeEx("Conversion", ImGuiTreeNodeFlags_DefaultOpen)) {
                Text("Pre Format Conversion:  %s", device->playback.converter.hasPreFormatConversion ? "YES" : "NO");
                Text("Post Format Conversion: %s", device->playback.converter.hasPostFormatConversion ? "YES" : "NO");
                Text("Channel Routing: %s", device->playback.converter.hasChannelConverter ? "YES" : "NO");
                Text("Resampling: %s", device->playback.converter.hasResampler ? "YES" : "NO");
                Text("Passthrough: %s", device->playback.converter.isPassthrough ? "YES" : "NO");
                {
                    char channel_map[1024];
                    ma_channel_map_to_string(device->playback.channelMap, device->playback.channels, channel_map, sizeof(channel_map));
                    Text("Channel Map In: {%s}", channel_map);

                    ma_channel_map_to_string(device->playback.internalChannelMap, device->playback.internalChannels, channel_map, sizeof(channel_map));
                    Text("Channel Map Out: {%s}", channel_map);
                }
                TreePop();
            }
            TreePop();
        }
        TreePop();
    }

    // const auto backend_count = soundio_backend_count(soundio);
    // if (TreeNodeEx("Backends", ImGuiTreeNodeFlags_None, "Available backends (%d)", backend_count)) {
    //     for (int i = 0; i < backend_count; i++) {
    //         const auto backend = soundio_get_backend(soundio, i);
    //         BulletText("%s%s", soundio_backend_name(backend), backend == soundio->current_backend ? " (current)" : "");
    //     }
    //     TreePop();
    // }
    // if (TreeNode("Plots")) {
    //     ShowBufferPlots();
    //     TreePop();
    // }
}

void Audio::Device::Uninit() const {
    ma_device_uninit(&MaDevice);
    // ma_resampler_uninit(&Resampler, nullptr);
}

void Audio::Device::Start() const {
    const int result = ma_device_start(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error starting audio device: {}", result));
}
void Audio::Device::Stop() const {
    const int result = ma_device_stop(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Error stopping audio device: {}", result));
}
bool Audio::Device::IsStarted() const { return ma_device_is_started(&MaDevice); }

void Audio::Graph::Init() const {
    NodeGraphConfig = ma_node_graph_config_init(MaDevice.capture.channels);
    int result = ma_node_graph_init(&NodeGraphConfig, nullptr, &NodeGraph);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize node graph: {}", result));

    Nodes.Init();
    vector<Primitive> connections{};
    Count dest_count = 0;
    for (const auto *dest_node : Nodes) {
        if (!dest_node->IsDestination()) continue;
        for (const auto *source_node : Nodes) {
            if (!source_node->IsSource()) continue;
            const bool default_connected =
                (source_node == &Nodes.Input && dest_node == &Nodes.Faust) ||
                (source_node == &Nodes.Faust && dest_node == &Nodes.Output);
            connections.push_back(default_connected);
        }
        dest_count++;
    }
    q(Action::SetMatrix{Connections.Path, connections, dest_count}, true);
}

void Audio::Graph::Update() const {
    Nodes.Update();

    // Setting up busses is idempotent.
    Count source_i = 0;
    for (const Node *source_node : Nodes) {
        if (!source_node->IsSource()) continue;
        ma_node_detach_output_bus(source_node->Get(), 0); // No way to just detach one connection.
        Count dest_i = 0;
        for (const Node *dest_node : Nodes) {
            if (!dest_node->IsDestination()) continue;
            if (Connections(dest_i, source_i)) {
                ma_node_attach_output_bus(source_node->Get(), 0, dest_node->Get(), 0);
            }
            dest_i++;
        }
        source_i++;
    }
}
void Audio::Graph::Uninit() const {
    Nodes.Uninit();
    // ma_node_graph_uninit(&NodeGraph, nullptr); // Graph endpoint is already uninitialized in `Nodes.Uninit`.
}
void Audio::Graph::Render() const {
    if (BeginTabBar("")) {
        if (BeginTabItem(Nodes.ImGuiLabel.c_str())) {
            Nodes.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Connections")) {
            RenderConnections();
            EndTabItem();
        }
        EndTabBar();
    }
}

void Audio::Graph::Nodes::Init() const {
    Output.Set(ma_node_graph_get_endpoint(&NodeGraph)); // Output is present whenever the graph is running. todo Graph is a Node
    for (const Node *node : *this) node->Init();
}
void Audio::Graph::Nodes::Update() const {
    for (const Node *node : *this) node->Update();
}
void Audio::Graph::Nodes::Uninit() const {
    for (const Node *node : *this) node->Uninit();
}
void Audio::Graph::Nodes::Render() const {
    for (const Node *node : *this) {
        if (TreeNodeEx(node->ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            node->Draw();
            TreePop();
        }
    }
}

void *Audio::Graph::Node::Get() const { return DataFor.contains(Id) ? DataFor.at(Id) : nullptr; }
void Audio::Graph::Node::Set(ma_node *data) const {
    if (data == nullptr) DataFor.erase(Id);
    else DataFor[Id] = data;
}

Count Audio::Graph::Node::InputBusCount() const { return ma_node_get_input_bus_count(Get()); }
Count Audio::Graph::Node::OutputBusCount() const { return ma_node_get_output_bus_count(Get()); }
Count Audio::Graph::Node::InputChannelCount(Count bus) const { return ma_node_get_input_channels(Get(), bus); }
Count Audio::Graph::Node::OutputChannelCount(Count bus) const { return ma_node_get_output_channels(Get(), bus); }

void Audio::Graph::Node::Init() const {
    DoInit();
    NeedsRestart(); // xxx Updates cached values as side effect.
}
void Audio::Graph::Node::DoInit() const {
}
void Audio::Graph::Node::Update() const {
    const bool is_initialized = Get() != nullptr;
    const bool needs_restart = NeedsRestart(); // Don't inline! Must run during every update.
    if (On && !is_initialized) {
        Init();
    } else if (!On && is_initialized) {
        Uninit();
    } else if (needs_restart && is_initialized) {
        Uninit();
        Init();
    }
    if (On) ma_node_set_output_bus_volume(Get(), 0, Volume);
}
void Audio::Graph::Node::Uninit() const {
    if (!Get()) return;

    DoUninit();
    Set(nullptr);
}
void Audio::Graph::Node::DoUninit() const {
    ma_node_uninit(Get(), nullptr);
}
void Audio::Graph::Node::Render() const {
    On.Draw();
    Volume.Draw();
}

// Output node is already allocated by the MA graph, so we don't need to track internal data for it.
void Audio::Graph::InputNode::DoInit() const {
    int result = ma_audio_buffer_ref_init(MaDevice.capture.format, MaDevice.capture.channels, nullptr, 0, &InputBuffer);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize input audio buffer: ", result));

    static ma_data_source_node Node{};
    static ma_data_source_node_config Config{};

    Config = ma_data_source_node_config_init(&InputBuffer);
    result = ma_data_source_node_init(&NodeGraph, &Config, nullptr, &Node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the input node: ", result));

    Set(&Node);
}
void Audio::Graph::InputNode::DoUninit() const {
    ma_data_source_node_uninit((ma_data_source_node *)Get(), nullptr);
    ma_audio_buffer_ref_uninit(&InputBuffer);
}

void FaustProcess(ma_node *node, const float **const_bus_frames_in, ma_uint32 *frame_count_in, float **bus_frames_out, ma_uint32 *frame_count_out) {
    // ma_pcm_rb_init_ex()
    // ma_deinterleave_pcm_frames()
    float **bus_frames_in = const_cast<float **>(const_bus_frames_in); // Faust `compute` expects a non-const buffer: https://github.com/grame-cncm/faust/pull/850
    if (FaustContext::Dsp) FaustContext::Dsp->compute(*frame_count_out, bus_frames_in, bus_frames_out);

    (void)node; // unused
    (void)frame_count_in; // unused
}

void Audio::Graph::FaustNode::DoInit() const {
    if (!FaustContext::Dsp) return;

    FaustContext::Dsp->init(audio.Device.SampleRate);
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
    const int result = ma_node_init(&NodeGraph, &config, nullptr, &node);
    if (result != MA_SUCCESS) throw std::runtime_error(std::format("Failed to initialize the Faust node: {}", result));

    Set(&node);
}
bool Audio::Graph::FaustNode::NeedsRestart() const {
    static dsp *PreviousDsp = FaustContext::Dsp;
    static U32 PreviousSampleRate = audio.Device.SampleRate;

    const bool needs_restart = FaustContext::Dsp != PreviousDsp || audio.Device.SampleRate != PreviousSampleRate;
    PreviousDsp = FaustContext::Dsp;
    PreviousSampleRate = audio.Device.SampleRate;

    return needs_restart;
}

// todo next up: Migrate all remaining `Audio` method definitions from `App.cpp` to here
//   This requires a solution for getting global `Style` here, either by extracting it from `App`,
//   or breaking it out into separate style members owned by their domain parents, e.g. `Audio::Style`.

void Audio::Faust::FaustLog::Render() const {
    PushStyleColor(ImGuiCol_Text, {1, 0, 0, 1});
    Error.Draw();
    PopStyleColor();
}

void Audio::Faust::Render() const {}

Audio::Graph::Node::Node(StateMember *parent, string_view path_segment, string_view name_help, bool on)
    : UIStateMember(parent, path_segment, name_help) {
    store::Set(On, on);
}

void Audio::Graph::RenderConnections() const {
    const auto &style = Style.Matrix;
    const float cell_size = style.CellSize * GetTextLineHeight();
    const float cell_gap = style.CellGap;
    const float label_size = style.LabelSize * GetTextLineHeight(); // Does not include padding.
    const float label_padding = ImGui::GetStyle().ItemInnerSpacing.x;
    const float max_label_w = label_size + 2 * label_padding;
    const ImVec2 grid_top_left = GetCursorScreenPos() + max_label_w;

    BeginGroup();
    // Draw the source channel labels.
    Count source_count = 0;
    for (const auto *source_node : Nodes) {
        if (!source_node->IsSource()) continue;

        const char *label = source_node->Name.c_str();
        const string ellipsified_label = Ellipsify(string(label), label_size);

        SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * source_count, -max_label_w});
        const auto label_interaction_flags = fg::InvisibleButton({cell_size, max_label_w}, source_node->ImGuiLabel.c_str());
        ImPlot::AddTextVertical(
            GetWindowDrawList(),
            grid_top_left + ImVec2{(cell_size + cell_gap) * source_count + (cell_size - GetTextLineHeight()) / 2, -label_padding},
            GetColorU32(ImGuiCol_Text), ellipsified_label.c_str()
        );
        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label);
        source_count++;
    }

    // Draw the destination channel labels and mixer cells.
    Count dest_i = 0;
    for (const auto *dest_node : Nodes) {
        if (!dest_node->IsDestination()) continue;

        const char *label = dest_node->Name.c_str();
        const string ellipsified_label = Ellipsify(string(label), label_size);

        SetCursorScreenPos(grid_top_left + ImVec2{-max_label_w, (cell_size + cell_gap) * dest_i});
        const auto label_interaction_flags = fg::InvisibleButton({max_label_w, cell_size}, dest_node->ImGuiLabel.c_str());
        const float label_w = CalcTextSize(ellipsified_label.c_str()).x;
        SetCursorPos(GetCursorPos() + ImVec2{max_label_w - label_w - label_padding, (cell_size - GetTextLineHeight()) / 2}); // Right-align & vertically center label.
        TextUnformatted(ellipsified_label.c_str());
        const bool text_clipped = ellipsified_label.find("...") != string::npos;
        if (text_clipped && (label_interaction_flags & InteractionFlags_Hovered)) SetTooltip("%s", label);

        for (Count source_i = 0; source_i < source_count; source_i++) {
            PushID(dest_i * source_count + source_i);
            SetCursorScreenPos(grid_top_left + ImVec2{(cell_size + cell_gap) * source_i, (cell_size + cell_gap) * dest_i});
            const auto flags = fg::InvisibleButton({cell_size, cell_size}, "Cell");
            if (flags & InteractionFlags_Clicked) q(Action::SetValue{Connections.PathAt(dest_i, source_i), !Connections(dest_i, source_i)});

            const auto fill_color = flags & InteractionFlags_Held ? ImGuiCol_ButtonActive : (flags & InteractionFlags_Hovered ? ImGuiCol_ButtonHovered : (Connections(dest_i, source_i) ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg));
            RenderFrame(GetItemRectMin(), GetItemRectMax(), GetColorU32(fill_color));
            PopID();
        }
        dest_i++;
    }
    EndGroup();
}

void Audio::Style::Render() const {
    if (BeginTabBar("Style")) {
        if (BeginTabItem("Matrix mixer", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Graph.Style.Matrix.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust graph", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.Graph.Style.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.Params.Style.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}
