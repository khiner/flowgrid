#include "Audio.h"

#include "imgui.h"

Audio::Audio(ArgsT &&args) : ActionableComponent(std::move(args)) {
    Graph.RegisterWindow();
    Graph.Connections.RegisterWindow();
    Style.RegisterWindow();
    Faust.FaustDsps.RegisterWindow();
    Faust.Logs.RegisterWindow();
    Faust.Graphs.RegisterWindow();
    Faust.Paramss.RegisterWindow();

    Faust.RegisterDspChangeListener(&Graph);
}

Audio::~Audio() {
    Faust.UnregisterDspChangeListener(&Graph);
}

void Audio::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::AudioGraph::Any &a) { Graph.Apply(a); },
            [this](const Action::Faust::DSP::Create &) { Faust.FaustDsps.EmplaceBack(FaustDspPathSegment); },
            [this](const Action::Faust::DSP::Delete &a) { Faust.FaustDsps.EraseId(a.id); },
            [this](const Action::Faust::Graph::Any &a) { Faust.Graphs.Apply(a); },
            [this](const Action::Faust::GraphStyle::ApplyColorPreset &a) {
                const auto &colors = Faust.Graphs.Style.Colors;
                switch (a.id) {
                    case 0: return colors.Set(FaustGraphStyle::ColorsDark);
                    case 1: return colors.Set(FaustGraphStyle::ColorsLight);
                    case 2: return colors.Set(FaustGraphStyle::ColorsClassic);
                    case 3: return colors.Set(FaustGraphStyle::ColorsFaust);
                }
            },
            [this](const Action::Faust::GraphStyle::ApplyLayoutPreset &a) {
                const auto &style = Faust.Graphs.Style;
                switch (a.id) {
                    case 0: return style.LayoutFlowGrid();
                    case 1: return style.LayoutFaust();
                }
            },

        },
        action
    );
}

bool Audio::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](const Action::AudioGraph::Any &a) { return Graph.CanApply(a); },
            [this](const Action::Faust::Graph::Any &a) { return Faust.Graphs.CanApply(a); },
            [](auto &&) { return true; },
        },
        action
    );
}

void Audio::Render() const {
    Faust.Draw();
}

using namespace ImGui;

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
