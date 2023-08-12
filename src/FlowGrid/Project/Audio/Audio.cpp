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
        [this](const Action::AudioGraph::Any &a) { Graph.Apply(a); },
        [this](const Action::Faust &a) { Faust.Apply(a); },
    );
}

bool Audio::CanApply(const ActionType &) const { return true; }

using namespace ImGui;

void Audio::Style::Render() const {
    const auto &audio = static_cast<const Audio &>(*Parent);
    if (BeginTabBar("")) {
        if (BeginTabItem("Matrix mixer", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Graph.Style.Matrix.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust graph", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.Graphs.Style.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.ParamsUis.Style.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}
