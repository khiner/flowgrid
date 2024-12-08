#include "Audio.h"

#include "imgui_internal.h"

Audio::Audio(ArgsT &&args) : ActionableComponent(std::move(args)) {
    Graph.RegisterWindow();
    Graph.Connections.RegisterWindow();
    Faust.FaustDsps.RegisterWindow();
    Faust.Logs.RegisterWindow();
    Faust.Graphs.RegisterWindow();
    Faust.Paramss.RegisterWindow();
    Style.RegisterWindow();

    Faust.RegisterDspChangeListener(_S, &Graph);
}

Audio::~Audio() {
    Faust.UnregisterDspChangeListener(&Graph);
}

void Audio::Apply(TransientStore &s, const ActionType &action) const {
    std::visit(
        Match{
            [this, &s](const Action::AudioGraph::Any &a) { Graph.Apply(s, a); },
            [this, &s](const Action::Faust::DSP::Create &) { Faust.FaustDsps.EmplaceBack(s, FaustDspPathSegment); },
            [this, &s](const Action::Faust::DSP::Delete &a) { Faust.FaustDsps.EraseId(s, a.id); },
            [this, &s](const Action::Faust::Graph::Any &a) { Faust.Graphs.Apply(s, a); },
            [this, &s](const Action::Faust::GraphStyle::ApplyColorPreset &a) {
                const auto &colors = Faust.Graphs.Style.Colors;
                switch (a.id) {
                    case 0: return colors.Set(s, FaustGraphStyle::ColorsDark);
                    case 1: return colors.Set(s, FaustGraphStyle::ColorsLight);
                    case 2: return colors.Set(s, FaustGraphStyle::ColorsClassic);
                    case 3: return colors.Set(s, FaustGraphStyle::ColorsFaust);
                }
            },
            [this, &s](const Action::Faust::GraphStyle::ApplyLayoutPreset &a) {
                const auto &style = Faust.Graphs.Style;
                switch (a.id) {
                    case 0: return style.LayoutFlowGrid(s);
                    case 1: return style.LayoutFaust(s);
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

void Audio::Dock(ID *node_id) const {
    auto flowgrid_node_id = DockBuilderSplitNode(*node_id, ImGuiDir_Left, 0.25f, nullptr, node_id);
    auto faust_tools_node_id = DockBuilderSplitNode(*node_id, ImGuiDir_Down, 0.5f, nullptr, node_id);
    auto faust_graph_node_id = DockBuilderSplitNode(faust_tools_node_id, ImGuiDir_Left, 0.5f, nullptr, &faust_tools_node_id);
    DockBuilderSplitNode(*node_id, ImGuiDir_Right, 0.5f, nullptr, node_id); // text editor

    Graph.Dock(&flowgrid_node_id);
    Graph.Connections.Dock(&flowgrid_node_id);
    Style.Dock(&flowgrid_node_id);
    Faust.FaustDsps.Dock(node_id);
    Faust.Graphs.Dock(&faust_graph_node_id);
    Faust.Paramss.Dock(&faust_tools_node_id);
    Faust.Logs.Dock(&faust_tools_node_id);
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
