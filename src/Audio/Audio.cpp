#include "Audio.h"

#include "imgui.h"

Audio::Audio(ArgsT &&args) : ActionProducerComponent(std::move(args)) {
    Faust.RegisterDspChangeListener(&Graph);
}

Audio::~Audio() {
    Faust.UnregisterDspChangeListener(&Graph);
}

using namespace ImGui;

void Audio::Render() const {
    Faust.Draw();
}

void Audio::Style::Render() const {
    if (BeginTabBar("")) {
        const auto &audio = static_cast<const Audio &>(*Parent);
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
