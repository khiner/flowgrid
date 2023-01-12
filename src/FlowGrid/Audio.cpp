// #include "CDSPResampler.h"
#include "Helper/Sample.h" // Must be included before any Faust includes
#include "faust/dsp/llvm-dsp.h"

#include "App.h"
#include "Helper/File.h"
#include "UI/Faust/FaustGraph.h"
#include "UI/Faust/FaustParams.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "implot_internal.h"

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
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio context: {}", result));

    static Count PlaybackDeviceCount, CaptureDeviceCount;
    static ma_device_info *PlaybackDeviceInfos, *CaptureDeviceInfos;
    result = ma_context_get_devices(&AudioContext, &PlaybackDeviceInfos, &PlaybackDeviceCount, &CaptureDeviceInfos, &CaptureDeviceCount);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio devices: {}", result));

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
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error shutting down audio context: {}", result));
}

// todo draw debug info for all devices, not just current
//  void DrawDevices() {
//      for (const IO io : IO_All) {
//          const Count device_count = GetDeviceCount(io);
//          if (TreeNodeEx(format("{} devices ({})", Capitalize(to_string(io)), device_count).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
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
    TabsWindow::Render();
}

namespace FaustContext {
static dsp *Dsp = nullptr;
static unique_ptr<FaustParams> Ui;

static void Init() {
    createLibContext();

    int argc = 0;
    const char **argv = new const char *[8];
    argv[argc++] = "-I";
    argv[argc++] = fs::relative("../lib/faust/libraries").c_str();
    if (std::is_same_v<Sample, double>) argv[argc++] = "-double";

    static int num_inputs, num_outputs;
    static string error_msg;
    const Box box = DSPToBoxes("FlowGrid", s.Faust.Code, argc, argv, &num_inputs, &num_outputs, error_msg);

    static llvm_dsp_factory *dsp_factory;
    if (box && error_msg.empty()) {
        static const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromBoxes("FlowGrid", box, argc, argv, "", error_msg, optimize_level);
    }
    if (!box && error_msg.empty()) error_msg = "`DSPToBoxes` returned no error but did not produce a result.";

    if (dsp_factory && error_msg.empty()) {
        Dsp = dsp_factory->createDSPInstance();
        if (!Dsp) error_msg = "Could not create Faust DSP.";
        else {
            Dsp->init(s.Audio.Device.SampleRate);
            Ui = make_unique<FaustParams>();
            Dsp->buildUserInterface(Ui.get());
        }
    }

    const auto &FaustLog = s.Faust.Log;
    if (!error_msg.empty()) q(SetValue{FaustLog.Error.Path, error_msg});
    else if (FaustLog.Error) q(SetValue{FaustLog.Error.Path, ""});

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
    static string PreviousFaustCode = s.Faust.Code;
    static U32 PreviousSampleRate = s.Audio.Device.SampleRate;

    const bool needs_restart = s.Faust.Code != PreviousFaustCode || s.Audio.Device.SampleRate != PreviousSampleRate;

    PreviousFaustCode = s.Faust.Code;
    PreviousSampleRate = s.Audio.Device.SampleRate;

    return needs_restart;
}
} // namespace FaustContext

void Audio::Update() const {
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

    const bool is_faust_initialized = s.UiProcess.Running && s.Faust.Code && !s.Faust.Log.Error;
    const bool faust_needs_restart = FaustContext::NeedsRestart(); // Don't inline! Must run during every update.
    if (!FaustContext::Dsp && is_faust_initialized) {
        FaustContext::Init();
    } else if (FaustContext::Dsp && !is_faust_initialized) {
        FaustContext::Uninit();
    } else if (faust_needs_restart) {
        FaustContext::Uninit();
        FaustContext::Init();
    }

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
    return ::format("{}{}", ma_get_format_name((ma_format)format), is_native ? "*" : "");
}
const string Audio::Device::GetSampleRateName(const U32 sample_rate) {
    const bool is_native = std::find(NativeSampleRates.begin(), NativeSampleRates.end(), sample_rate) != NativeSampleRates.end();
    return format("{}{}", to_string(sample_rate), is_native ? "*" : "");
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
    // if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing resampler: {}", result));
    // ResamplerConfig.pBackendVTable = &ResamplerVTable;

    int result = ma_device_init(nullptr, &DeviceConfig, &MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error initializing audio device: {}", result));

    result = ma_context_get_device_info(MaDevice.pContext, MaDevice.type, nullptr, &DeviceInfo);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error getting audio device info: {}", result));

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
    if (!initial_settings.empty()) q(SetValues{initial_settings}, true);
}

void Audio::Device::Update() const {
    if (IsStarted()) ma_device_set_master_volume(&MaDevice, Volume);
}

using namespace ImGui;

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
        TextUnformatted(Capitalize(to_string(io)).c_str());
        (io == IO_In ? InDeviceName : OutDeviceName).Render(DeviceNames[io]);
        // (io == IO_In ? InFormat : OutFormat).Render(PrioritizedFormats); // See above - always using f32 format.
    }
    if (TreeNode("Info")) {
        auto *device = &MaDevice;
        assert(device->type == ma_device_type_duplex || device->type == ma_device_type_loopback);

        Text("[%s]", ma_get_backend_name(device->pContext->backend));

        static char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        ma_device_get_name(device, device->type == ma_device_type_loopback ? ma_device_type_playback : ma_device_type_capture, name, sizeof(name), nullptr);
        if (TreeNode(format("{} ({})", name, "Capture").c_str())) {
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
        if (TreeNode(format("{} ({})", name, "Playback").c_str())) {
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
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error starting audio device: {}", result));
}
void Audio::Device::Stop() const {
    const int result = ma_device_stop(&MaDevice);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Error stopping audio device: {}", result));
}
bool Audio::Device::IsStarted() const { return ma_device_is_started(&MaDevice); }

void Audio::Graph::Init() const {
    NodeGraphConfig = ma_node_graph_config_init(MaDevice.capture.channels);
    int result = ma_node_graph_init(&NodeGraphConfig, nullptr, &NodeGraph);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize node graph: {}", result));

    result = ma_audio_buffer_ref_init(MaDevice.capture.format, MaDevice.capture.channels, nullptr, 0, &InputBuffer);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize input audio buffer: ", result));

    Nodes.Init();
    vector<Primitive> connections{};
    for (const auto *output_node : Nodes.Children) {
        for (const auto *input_node : Nodes.Children) {
            const bool default_connected =
                (input_node == &Nodes.Input && output_node == &Nodes.Faust) ||
                (input_node == &Nodes.Faust && output_node == &Nodes.Output);
            connections.push_back(default_connected);
        }
    }
    q(SetMatrix{Connections.Path, connections, Count(Nodes.Children.size())}, true);
}

void Audio::Graph::Update() const {
    Nodes.Update();
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

void Audio::Graph::Nodes::Update() const {
    Output.Set(ma_node_graph_get_endpoint(&NodeGraph)); // Output is present whenever the graph is running. todo Graph is a Node

    for (const auto *child : Children) dynamic_cast<const Node *>(child)->Update();

    // Setting up busses is idempotent.
    const auto *graph = dynamic_cast<const Graph *>(Parent);
    for (Count i = 0; i < Children.size(); i++) {
        if (auto *output_node = dynamic_cast<const Node *>(Children[i])->Get()) {
            ma_node_detach_output_bus(output_node, 0); // No way to just detach one connection.
            for (Count j = 0; j < Children.size(); j++) {
                if (auto *input_node = dynamic_cast<const Node *>(Children[j])->Get()) {
                    if (graph->Connections(i, j)) {
                        ma_node_attach_output_bus(input_node, 0, output_node, 0);
                    }
                }
            }
        }
    }
}

void Audio::Graph::Nodes::Init() const {
    for (const auto *child : Children) dynamic_cast<const Node *>(child)->Init();
}

void Audio::Graph::Nodes::Uninit() const {
    for (const auto *child : Children) dynamic_cast<const Node *>(child)->Uninit();
}
void Audio::Graph::Nodes::Render() const {
    for (const auto *child : Children) {
        const auto *node = dynamic_cast<const Node *>(child);
        if (TreeNodeEx(node->ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            node->Draw();
            TreePop();
        }
    }
}

unordered_map<ID, void *> Audio::Graph::Node::DataFor;

Audio::Graph::Node::Node(StateMember *parent, string_view path_segment, string_view name_help, bool on)
    : UIStateMember(parent, path_segment, name_help) {
    ::Set(On, on, c.InitStore);
}

void *Audio::Graph::Node::Get() const { return DataFor.contains(Id) ? DataFor.at(Id) : nullptr; }
void Audio::Graph::Node::Set(ma_node *data) const {
    if (data == nullptr) DataFor.erase(Id);
    else DataFor[Id] = data;
}

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
    static ma_data_source_node Node{};
    static ma_data_source_node_config Config{};

    Config = ma_data_source_node_config_init(&InputBuffer);

    int result = ma_data_source_node_init(&NodeGraph, &Config, nullptr, &Node);
    if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize the input node: ", result));

    Set(&Node);
}
void Audio::Graph::InputNode::DoUninit() const {
    ma_data_source_node_uninit((ma_data_source_node *)Get(), nullptr);
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
    static ma_node_base Node{};
    static ma_node_config Config{};
    static ma_node_vtable Vtable{};

    if (FaustContext::Dsp && FaustContext::Dsp->getNumInputs() > 0 && FaustContext::Dsp->getNumOutputs() > 0) {
        // todo called twice when restarting audio due to sample rate/format change
        Config = ma_node_config_init();
        Vtable = {FaustProcess, nullptr, 1, 1, 0};
        Config.vtable = &Vtable;
        Config.pInputChannels = (U32[]){U32(FaustContext::Dsp->getNumInputs())}; // One input bus, with N channels.
        Config.pOutputChannels = (U32[]){U32(FaustContext::Dsp->getNumOutputs())}; // One output bus, with M channels.

        const int result = ma_node_init(&NodeGraph, &Config, nullptr, &Node);
        if (result != MA_SUCCESS) throw std::runtime_error(format("Failed to initialize the Faust node: {}", result));

        Set(&Node);
    }
}
bool Audio::Graph::FaustNode::NeedsRestart() const {
    static dsp *PreviousDsp = FaustContext::Dsp;

    const bool needs_restart = PreviousDsp != FaustContext::Dsp;
    PreviousDsp = FaustContext::Dsp;

    return needs_restart;
}
// todo Graph::RenderConnections defined in `State.cpp` because of dependency resolution order weirdness.
