#include "Audio.h"

#include "imgui.h"

// todo support loopback mode? (think of use cases)
// todo explicit re-scan action.

Audio::Audio(ComponentArgs &&args) : Component(std::move(args)) {
    Graph.Nodes.Faust.OnDspChanged(Faust.Dsp);
    Graph.Update();

    Faust.Code.RegisterChangeListener(this);
}

Audio::~Audio() {
    Field::UnregisterChangeListener(this);
}

void Audio::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust &a) { Faust.Apply(a); },
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
        Graph.Nodes.Faust.Init();
        Graph.Update();
    }
}

// static ma_resampler_config ResamplerConfig;
// static ma_resampler Resampler;

using namespace ImGui;

void Audio::Render() const {
    Faust.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem(Device.ImGuiLabel.c_str())) {
            Device.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Graph.Nodes.ImGuiLabel.c_str())) {
            Graph.Nodes.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Graph.Connections.ImGuiLabel.c_str())) {
            Graph.RenderConnections();
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
