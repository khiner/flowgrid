#include "Audio.h"

#include "imgui.h"

Audio::Audio(ComponentArgs &&args) : Component(std::move(args)) {
    Faust.FaustDsps.RegisterDspChangeListener(&Graph);
}

Audio::~Audio() {
    Faust.FaustDsps.UnregisterDspChangeListener(&Graph);
}

void Audio::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::AudioGraph::Any &a) { Graph.Apply(a); },
        [this](const Action::Faust::Any &a) { Faust.Apply(a); },
    );
}

bool Audio::CanApply(const ActionType &action) const {
    return Visit(
        action,
        [this](const Action::AudioGraph::Any &a) { return Graph.CanApply(a); },
        [this](const Action::Faust::Any &a) { return Faust.CanApply(a); },
    );
}

using namespace ImGui;

void Audio::Render() const {
    Faust.Draw();
}

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
