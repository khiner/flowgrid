#include "Audio.h"

#include "imgui.h"

// todo support loopback mode? (think of use cases)
// todo explicit re-scan action.

Audio::Audio(ComponentArgs &&args) : Component(std::move(args)) {
    Faust.FaustDsp.RegisterDspChangeListener(&Graph);
}

Audio::~Audio() {
    Faust.FaustDsp.UnregisterDspChangeListener(&Graph);
}

void Audio::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust &a) { Faust.Apply(a); },
    );
}

bool Audio::CanApply(const ActionType &) const { return true; }

using namespace ImGui;

void Audio::Render() const {
    Faust.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem(Graph.InputDevice.ImGuiLabel.c_str())) {
            Graph.InputDevice.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Graph.OutputDevice.ImGuiLabel.c_str())) {
            Graph.OutputDevice.Draw();
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
