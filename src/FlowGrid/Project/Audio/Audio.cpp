#include "Audio.h"

#include "imgui.h"

Audio::Audio(ComponentArgs &&args, const ::FileDialog &file_dialog) : Component(std::move(args)), FileDialog(file_dialog) {
    Faust.RegisterDspChangeListener(&Graph);
}

Audio::~Audio() {
    Faust.UnregisterDspChangeListener(&Graph);
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
            audio.Faust.GraphStyle.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            audio.Faust.ParamsStyle.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}
