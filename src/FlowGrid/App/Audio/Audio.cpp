#include "Audio.h"

#include "imgui.h"

// todo support loopback mode? (think of use cases)
// todo explicit re-scan action.

Audio::Audio(ComponentArgs &&args) : Component(std::move(args)) {
    Graph.Nodes.Faust.OnDspChanged(Faust.Dsp);
    Graph.Update();

    Field::RegisterChangeListener(Faust.Code, this);
}

Audio::~Audio() {
    Field::UnregisterChangeListener(this);
}

void Audio::Apply(const ActionType &action) const {
    Visit(
        action,
        [&](const Action::Faust &a) { Faust.Apply(a); },
    );
}

bool Audio::CanApply(const ActionType &) const { return true; }

void Audio::AudioCallback(ma_device *device, void *output, const void *input, Count frame_count) {
    AudioGraph::AudioCallback(device, output, input, frame_count);
}

void Audio::OnFieldChanged() {
    // xxx this is obviously not great. could maybe move faust node management to `Faust`.
    if (Faust.Code.IsChanged()) {
        Graph.Nodes.Faust.Uninit();
        Faust.UpdateDsp();
        Graph.Nodes.Faust.OnDspChanged(Faust.Dsp);
        Graph.Nodes.Faust.Init(Graph.Get());
        Graph.Update();
    }
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

using namespace ImGui;

void Audio::Render() const {
    Faust.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem(Device.ImGuiLabel.c_str())) {
            Device.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Graph.ImGuiLabel.c_str())) {
            Graph.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Style")) {
            if (BeginTabBar("")) {
                if (BeginTabItem("Matrix mixer", nullptr, ImGuiTabItemFlags_NoPushId)) {
                    Graph.Style.Matrix.Draw();
                    EndTabItem();
                }
                if (BeginTabItem("Faust graph", nullptr, ImGuiTabItemFlags_NoPushId)) {
                    Faust.Graph.Style.Draw();
                    EndTabItem();
                }
                if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
                    Faust.Params.Style.Draw();
                    EndTabItem();
                }
                EndTabBar();
            }
            EndTabItem();
        }
        EndTabBar();
    }
}
