#include "Audio.h"

#include "imgui.h"

// todo support loopback mode? (think of use cases)
// todo explicit re-scan action.

static const Audio *Singleton;

Audio::Audio(ComponentArgs &&args) : Component(std::move(args)) {
    Faust.FaustDsp.RegisterDspChangeListener(&Graph);
    Singleton = this;
}

Audio::~Audio() {
    Singleton = nullptr;
    Faust.FaustDsp.UnregisterDspChangeListener(&Graph);
}

void Audio::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust &a) { Faust.Apply(a); },
    );
}

bool Audio::CanApply(const ActionType &) const { return true; }

void Audio::AudioInputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
    if (Singleton) Singleton->Graph.AudioInputCallback(device, output, input, frame_count);
}
void Audio::AudioOutputCallback(ma_device *device, void *output, const void *input, u32 frame_count) {
    if (Singleton) Singleton->Graph.AudioOutputCallback(device, output, input, frame_count);
}

using namespace ImGui;

void Audio::Render() const {
    Faust.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem(InputDevice.ImGuiLabel.c_str())) {
            InputDevice.Draw();
            EndTabItem();
        }
        if (BeginTabItem(OutputDevice.ImGuiLabel.c_str())) {
            OutputDevice.Draw();
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
