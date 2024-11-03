#include "ProjectCore.h"

#include "imgui_internal.h"
#include "implot.h"

#include "Core/UI/JsonTree.h"

#include "Project/ProjectContext.h"

using Style = flowgrid::Style;

ProjectCore::ProjectCore(ArgsT &&args) : ActionableComponent(std::move(args)) {
    Style.RegisterWindow();
    Demo.RegisterWindow();
    Info.RegisterWindow();
    Settings.RegisterWindow();

    Debug.RegisterWindow();
    Debug.StatePreview.RegisterWindow();
    Debug.StorePathUpdateFrequency.RegisterWindow();
    Debug.DebugLog.RegisterWindow();
    Debug.StackTool.RegisterWindow();
    Debug.Metrics.RegisterWindow();
}

void ProjectCore::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Windows::ToggleVisible &a) { Windows.ToggleVisible(a.component_id); },
            [this](const Action::Windows::ToggleDebug &a) {
                const bool toggling_on = !Windows.VisibleComponentIds.Contains(a.component_id);
                Windows.ToggleVisible(a.component_id);
                if (!toggling_on) return;

                auto *debug_component = static_cast<DebugComponent *>(Component::ById.at(a.component_id));
                if (auto *window = debug_component->FindDockWindow()) {
                    auto docknode_id = window->DockId;
                    auto debug_node_id = ImGui::DockBuilderSplitNode(docknode_id, ImGuiDir_Right, debug_component->SplitRatio, nullptr, &docknode_id);
                    debug_component->Dock(&debug_node_id);
                }
            },
            [this](const Action::Style::SetImGuiColorPreset &a) {
                const auto &colors = Style.ImGui.Colors;
                // todo enum types instead of raw int keys
                switch (a.id) {
                    case 0: return colors.Set(Style::ImGuiStyle::ColorsDark);
                    case 1: return colors.Set(Style::ImGuiStyle::ColorsLight);
                    case 2: return colors.Set(Style::ImGuiStyle::ColorsClassic);
                }
            },
            [this](const Action::Style::SetImPlotColorPreset &a) {
                const auto &style = Style.ImPlot;
                const auto &colors = style.Colors;
                switch (a.id) {
                    case 0:
                        colors.Set(Style::ImPlotStyle::ColorsAuto);
                        return style.MinorAlpha.Set(0.25f);
                    case 1:
                        colors.Set(Style::ImPlotStyle::ColorsDark);
                        return style.MinorAlpha.Set(0.25f);
                    case 2:
                        colors.Set(Style::ImPlotStyle::ColorsLight);
                        return style.MinorAlpha.Set(1);
                    case 3:
                        colors.Set(Style::ImPlotStyle::ColorsClassic);
                        return style.MinorAlpha.Set(0.5f);
                }
            },
            [this](const Action::Style::SetProjectColorPreset &a) {
                const auto &colors = Style.Project.Colors;
                switch (a.id) {
                    case 0: return colors.Set(ProjectStyle::ColorsDark);
                    case 1: return colors.Set(ProjectStyle::ColorsLight);
                    case 2: return colors.Set(ProjectStyle::ColorsClassic);
                }
            },
        },
        action
    );
}

bool ProjectCore::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [](auto &&) { return true; },
        },
        action
    );
}

void ProjectCore::FocusDefault() const {
    Style.Focus();
    Debug.Focus(); // not visible by default anymore
}

using namespace ImGui;

void ProjectCore::Dock(ID *node_id) const {
    auto debug_node_id = DockBuilderSplitNode(*node_id, ImGuiDir_Down, 0.3f, nullptr, node_id);
    auto metrics_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Right, 0.3f, nullptr, &debug_node_id);
    auto utilities_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Left, 0.3f, nullptr, &debug_node_id);
    auto info_node_id = DockBuilderSplitNode(*node_id, ImGuiDir_Right, 0.2f, nullptr, node_id);
    auto settings_node_id = DockBuilderSplitNode(info_node_id, ImGuiDir_Down, 0.25f, nullptr, &info_node_id);

    Style.Dock(&utilities_node_id);
    Demo.Dock(&utilities_node_id);
    Info.Dock(&info_node_id);
    Settings.Dock(&settings_node_id);
    Debug.Dock(&debug_node_id);
    Debug.StatePreview.Dock(&debug_node_id);
    Debug.StorePathUpdateFrequency.Dock(&debug_node_id);
    Debug.DebugLog.Dock(&debug_node_id);
    Debug.StackTool.Dock(&debug_node_id);
    Debug.Metrics.Dock(&metrics_node_id);
}

void ProjectCore::RenderDebug() const {
    const bool auto_select = Debug.AutoSelect;
    if (auto_select) BeginDisabled();
    RenderValueTree(Debug::LabelModeType(int(Debug.LabelMode)) == Debug::LabelModeType::Annotated, auto_select);
    if (auto_select) EndDisabled();
}

void ProjectCore::Debug::StorePathUpdateFrequency::Render() const {
    Ctx.RenderStorePathChangeFrequency();
}

// todo only overriding to draw self in addition to children - refactor to avoid this
void ProjectCore::Debug::DrawWindowsMenu() const {
    const auto &item = Ctx.DrawMenuItem;
    if (BeginMenu(Name.c_str())) {
        item(*this);
        item(StatePreview);
        item(StorePathUpdateFrequency);
        item(DebugLog);
        item(StackTool);
        item(Metrics);
        EndMenu();
    }
}

void ProjectCore::Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void ProjectCore::Debug::StackTool::Render() const {
    ShowIDStackToolWindow();
}

void ProjectCore::Debug::Metrics::ImGuiMetrics::Render() const { ImGui::ShowMetricsWindow(); }
void ProjectCore::Debug::Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

void ProjectCore::Debug::OnComponentChanged() {
    if (AutoSelect.IsChanged()) {
        WindowFlags = AutoSelect ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None;
    }
}

void ProjectCore::Debug::StatePreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    json project_json = Ctx.GetProjectJson(ProjectFormat(int(Format)));
    if (Raw) {
        TextUnformatted(project_json.dump(4));
    } else {
        SetNextItemOpen(true);
        flowgrid::JsonTree("", std::move(project_json));
    }
}

void ProjectCore::Debug::Metrics::Render() const {
    RenderTabs();
}

void ProjectCore::Debug::Metrics::ProjectMetrics::Render() const {
    Ctx.RenderMetrics();
}