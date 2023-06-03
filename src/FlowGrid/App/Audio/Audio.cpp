#include "Audio.h"

#include <range/v3/core.hpp>
#include <range/v3/view/transform.hpp>

#include "miniaudio.h"

#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"

#include "App/FileDialog/FileDialog.h"
#include "Faust/FaustBox.h"
#include "Helper/File.h"
#include "Helper/String.h"
#include "UI/Widgets.h"

static const std::string FaustDspFileExtension = ".dsp";

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

void Audio::Apply(const Action::AudioAction &action) const {
    using namespace Action;
    Match(
        action,
        [&](const SetGraphColorStyle &a) {
            switch (a.id) {
                case 0: return Faust.Graph.Style.ColorsDark();
                case 1: return Faust.Graph.Style.ColorsLight();
                case 2: return Faust.Graph.Style.ColorsClassic();
                case 3: return Faust.Graph.Style.ColorsFaust();
            }
        },
        [&](const SetGraphLayoutStyle &a) {
            switch (a.id) {
                case 0: return Faust.Graph.Style.LayoutFlowGrid();
                case 1: return Faust.Graph.Style.LayoutFaust();
            }
        },
        [&](const ShowOpenFaustFileDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", ""}); },
        [&](const ShowSaveFaustFileDialog &) { file_dialog.Set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1}); },
        [&](const ShowSaveFaustSvgFileDialog &) { file_dialog.Set({"Choose directory", ".*", ".", "faust_graph", true, 1}); },
        [&](const OpenFaustFile &a) { store::Set(Faust.Code, FileIO::read(a.path)); },
        [&](const SaveFaustFile &a) { FileIO::write(a.path, audio.Faust.Code); },
        [](const SaveFaustSvgFile &a) { SaveBoxSvg(a.path); },
    );
}

// static ma_resampler_config ResamplerConfig;
// static ma_resampler Resampler;

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

    static string PrevSelectedPath = "";
    if (PrevSelectedPath != file_dialog.SelectedFilePath) {
        const fs::path selected_path = string(file_dialog.SelectedFilePath);
        const string &extension = selected_path.extension();
        if (extension == FaustDspFileExtension) {
            if (file_dialog.SaveMode) Action::SaveFaustFile{selected_path}.q();
            else Action::OpenFaustFile{selected_path}.q();
        } else if (extension == ".svg") {
            if (file_dialog.SaveMode) Action::SaveFaustSvgFile{selected_path}.q();
        }
        PrevSelectedPath = selected_path;
    }
}

// todo support loopback mode? (think of use cases)

// todo explicit re-scan action.
void Audio::Init() const {
    Device.Init(AudioGraph::AudioCallback);
    Graph.Init();
    Device.Start();

    Device.NeedsRestart(); // xxx Updates cached values as side effect.
}

void Audio::Uninit() const {
    Device.Stop();
    Graph.Uninit();
    Device.Uninit();
}

void Audio::Update() const {
    const bool is_initialized = Device.IsStarted();
    const bool needs_restart = Device.NeedsRestart(); // Don't inline! Must run during every update.
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

using namespace ImGui;

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
