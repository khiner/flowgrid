#include "State.h"

#include "imgui_internal.h"
#include "implot.h"

#include "Core/Store/Store.h"

#include "ProjectContext.h"

#include "UI/JsonTree.h"

using namespace FlowGrid;

State::State(Store &store, ActionableProducer::EnqueueFn q, const ::ProjectContext &project_context)
    : Component(store, PrimitiveQ, Windows, Style), ActionableProducer(std::move(q)), ProjectContext(project_context) {
    Windows.SetWindowComponents({
        Audio.Graph,
        Audio.Graph.Connections,
        Audio.Style,
        Settings,
        Audio.Faust.FaustDsps,
        Audio.Faust.Logs,
        Audio.Faust.Graphs,
        Audio.Faust.Paramss,
        Debug,
        Debug.StatePreview,
        Debug.StorePathUpdateFrequency,
        Debug.DebugLog,
        Debug.StackTool,
        Debug.Metrics,
        Style,
        Demo,
        Info,
    });
}

State::~State() = default;

void ApplyVectorSet(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::flex_vector<decltype(a.value)>>(a.component_id).set(a.i, a.value));
}
void ApplySetInsert(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::set<decltype(a.value)>>(a.component_id).insert(a.value));
}
void ApplySetErase(Store &s, const auto &a) {
    s.Set(a.component_id, s.Get<immer::set<decltype(a.value)>>(a.component_id).erase(a.value));
}

void State::Apply(const ActionType &action) const {
    std::visit(
        Match{
            /* Primitives */
            [this](const Action::Primitive::Bool::Toggle &a) { _S.Set(a.component_id, !S.Get<bool>(a.component_id)); },
            [this](const Action::Primitive::Int::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::UInt::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Float::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Enum::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::Flags::Set &a) { _S.Set(a.component_id, a.value); },
            [this](const Action::Primitive::String::Set &a) { _S.Set(a.component_id, a.value); },
            /* Containers */
            [this](const Action::Container::Any &a) {
                const auto *container = Component::ById.at(a.GetComponentId());
                std::visit(
                    Match{
                        [container](const Action::AdjacencyList::ToggleConnection &a) {
                            const auto *al = static_cast<const AdjacencyList *>(container);
                            if (al->IsConnected(a.source, a.destination)) al->Disconnect(a.source, a.destination);
                            else al->Connect(a.source, a.destination);
                        },
                        [this, container](const Action::Vec2::Set &a) {
                            const auto *vec2 = static_cast<const Vec2 *>(container);
                            _S.Set(vec2->X.Id, a.value.first);
                            _S.Set(vec2->Y.Id, a.value.second);
                        },
                        [this, container](const Action::Vec2::SetX &a) { _S.Set(static_cast<const Vec2 *>(container)->X.Id, a.value); },
                        [this, container](const Action::Vec2::SetY &a) { _S.Set(static_cast<const Vec2 *>(container)->Y.Id, a.value); },
                        [this, container](const Action::Vec2::SetAll &a) {
                            const auto *vec2 = static_cast<const Vec2 *>(container);
                            _S.Set(vec2->X.Id, a.value);
                            _S.Set(vec2->Y.Id, a.value);
                        },
                        [this, container](const Action::Vec2::ToggleLinked &) {
                            const auto *vec2 = static_cast<const Vec2Linked *>(container);
                            _S.Set(vec2->Linked.Id, !S.Get<bool>(vec2->Linked.Id));
                            const float x = S.Get<float>(vec2->X.Id);
                            const float y = S.Get<float>(vec2->Y.Id);
                            if (x < y) _S.Set(vec2->Y.Id, x);
                            else if (y < x) _S.Set(vec2->X.Id, y);
                        },
                        [this](const Action::Vector<bool>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<int>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<u32>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<float>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Vector<std::string>::Set &a) { ApplyVectorSet(_S, a); },
                        [this](const Action::Set<u32>::Insert &a) { ApplySetInsert(_S, a); },
                        [this](const Action::Set<u32>::Erase &a) { ApplySetErase(_S, a); },
                        [this, container](const Action::Navigable<u32>::Clear &) {
                            const auto *nav = static_cast<const Navigable<u32> *>(container);
                            _S.Set<immer::flex_vector<u32>>(nav->Value.Id, {});
                            _S.Set(nav->Cursor.Id, 0);
                        },
                        [this, container](const Action::Navigable<u32>::Push &a) {
                            const auto *nav = static_cast<const Navigable<u32> *>(container);
                            const auto vec = S.Get<immer::flex_vector<u32>>(nav->Value.Id).push_back(a.value);
                            _S.Set<immer::flex_vector<u32>>(nav->Value.Id, vec);
                            _S.Set<u32>(nav->Cursor.Id, vec.size() - 1);
                        },

                        [this, container](const Action::Navigable<u32>::MoveTo &a) {
                            const auto *nav = static_cast<const Navigable<u32> *>(container);
                            auto cursor = u32(std::clamp(int(a.index), 0, int(S.Get<immer::flex_vector<u32>>(nav->Value.Id).size()) - 1));
                            _S.Set(nav->Cursor.Id, std::move(cursor));
                        },
                    },
                    a
                );
            },
            [](const Action::TextBuffer::Any &a) {
                const auto *buffer = Component::ById.at(a.GetComponentId());
                static_cast<const TextBuffer *>(buffer)->Apply(a);
            },
            [this](const Action::Store::ApplyPatch &a) {
                for (const auto &[id, ops] : a.patch.Ops) {
                    for (const auto &op : ops) {
                        if (op.Op == PatchOpType::PopBack) {
                            std::visit(
                                [&](auto &&v) {
                                    const auto vec = S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id);
                                    _S.Set(id, vec.take(vec.size() - 1));
                                },
                                *op.Old
                            );
                        } else if (op.Op == PatchOpType::Remove) {
                            std::visit([&](auto &&v) { _S.Erase<std::decay_t<decltype(v)>>(id); }, *op.Old);
                        } else if (op.Op == PatchOpType::Add || op.Op == PatchOpType::Replace) {
                            std::visit([&](auto &&v) { _S.Set(id, std::move(v)); }, *op.Value);
                        } else if (op.Op == PatchOpType::PushBack) {
                            std::visit([&](auto &&v) { _S.Set(id, S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).push_back(std::move(v))); }, *op.Value);
                        } else if (op.Op == PatchOpType::Set) {
                            std::visit([&](auto &&v) { _S.Set(id, S.Get<immer::flex_vector<std::decay_t<decltype(v)>>>(id).set(*op.Index, std::move(v))); }, *op.Value);
                        } else {
                            // `set` ops - currently, u32 is the only set value type.
                            std::visit(
                                Match{
                                    [&](u32 v) {
                                        if (op.Op == PatchOpType::Insert) _S.Set(id, S.Get<immer::set<decltype(v)>>(id).insert(v));
                                        else if (op.Op == PatchOpType::Erase) _S.Set(id, S.Get<immer::set<decltype(v)>>(id).erase(v));
                                    },
                                    [](auto &&) {},
                                },
                                *op.Value
                            );
                        }
                    }
                }
            },
            /* Audio */
            [this](const Action::AudioGraph::Any &a) { Audio.Graph.Apply(a); },
            [this](const Action::Faust::DSP::Create &) { Audio.Faust.FaustDsps.EmplaceBack(FaustDspPathSegment); },
            [this](const Action::Faust::DSP::Delete &a) { Audio.Faust.FaustDsps.EraseId(a.id); },
            [this](const Action::Faust::Graph::Any &a) { Audio.Faust.Graphs.Apply(a); },
            [this](const Action::Faust::GraphStyle::ApplyColorPreset &a) {
                const auto &colors = Audio.Faust.Graphs.Style.Colors;
                switch (a.id) {
                    case 0: return colors.Set(FaustGraphStyle::ColorsDark);
                    case 1: return colors.Set(FaustGraphStyle::ColorsLight);
                    case 2: return colors.Set(FaustGraphStyle::ColorsClassic);
                    case 3: return colors.Set(FaustGraphStyle::ColorsFaust);
                }
            },
            [this](const Action::Faust::GraphStyle::ApplyLayoutPreset &a) {
                const auto &style = Audio.Faust.Graphs.Style;
                switch (a.id) {
                    case 0: return style.LayoutFlowGrid();
                    case 1: return style.LayoutFaust();
                }
            },
            [this](const Action::Windows::ToggleVisible &a) { Windows.ToggleVisible(a.component_id); },
            [this](const Action::Windows::ToggleDebug &a) {
                const bool toggling_on = !Windows.VisibleComponents.Contains(a.component_id);
                Windows.ToggleVisible(a.component_id);
                if (!toggling_on) return;

                auto *debug_component = static_cast<DebugComponent *>(Component::ById.at(a.component_id));
                if (auto *window = debug_component->FindDockWindow()) {
                    auto docknode_id = window->DockId;
                    auto debug_node_id = ImGui::DockBuilderSplitNode(docknode_id, ImGuiDir_Right, debug_component->SplitRatio, nullptr, &docknode_id);
                    debug_component->Dock(debug_node_id);
                }
            },
            [this](const Action::Style::SetImGuiColorPreset &a) {
                // todo enum types instead of raw int keys
                switch (a.id) {
                    case 0: return Style.ImGui.Colors.Set(Style::ImGuiStyle::ColorsDark);
                    case 1: return Style.ImGui.Colors.Set(Style::ImGuiStyle::ColorsLight);
                    case 2: return Style.ImGui.Colors.Set(Style::ImGuiStyle::ColorsClassic);
                }
            },
            [this](const Action::Style::SetImPlotColorPreset &a) {
                switch (a.id) {
                    case 0:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsAuto);
                        return Style.ImPlot.MinorAlpha.Set(0.25f);
                    case 1:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsDark);
                        return Style.ImPlot.MinorAlpha.Set(0.25f);
                    case 2:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsLight);
                        return Style.ImPlot.MinorAlpha.Set(1);
                    case 3:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsClassic);
                        return Style.ImPlot.MinorAlpha.Set(0.5f);
                }
            },
            [this](const Action::Style::SetFlowGridColorPreset &a) {
                switch (a.id) {
                    case 0: return Style.FlowGrid.Colors.Set(FlowGridStyle::ColorsDark);
                    case 1: return Style.FlowGrid.Colors.Set(FlowGridStyle::ColorsLight);
                    case 2: return Style.FlowGrid.Colors.Set(FlowGridStyle::ColorsClassic);
                }
            },
            [](auto &&) {}, // All other actions are project actions.
        },
        action
    );
}

bool State::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](const Action::AudioGraph::Any &a) { return Audio.Graph.CanApply(a); },
            [this](const Action::Faust::Graph::Any &a) { return Audio.Faust.Graphs.CanApply(a); },
            [](auto &&) { return true; }, // All other actions are always allowed.
        },
        action
    );
}

using namespace ImGui;

bool IsPressed(ImGuiKeyChord chord) {
    return IsKeyChordPressed(chord, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner);
}

std::optional<State::ProducedActionType> ProduceKeyboardAction() {
    using namespace Action::Project;

    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_N)) return OpenEmpty{};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_O)) return ShowOpenDialog{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_S)) return ShowSaveDialog{};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) return Undo{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_Z)) return Redo{};
    if (IsPressed(ImGuiMod_Shift | ImGuiMod_Ctrl | ImGuiKey_O)) return OpenDefault{};
    if (IsPressed(ImGuiMod_Ctrl | ImGuiKey_S)) return SaveCurrent{};

    return {};
}

void State::Render() const {
    // Good initial layout setup example in this issue: https://github.com/ocornut/imgui/issues/3548
    auto dockspace_id = DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    int frame_count = GetCurrentContext()->FrameCount;
    if (frame_count == 1) {
        auto audio_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        auto utilities_node_id = DockBuilderSplitNode(audio_node_id, ImGuiDir_Down, 0.5f, nullptr, &audio_node_id);

        auto debug_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
        auto metrics_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Right, 0.35f, nullptr, &debug_node_id);

        auto info_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);
        auto settings_node_id = DockBuilderSplitNode(info_node_id, ImGuiDir_Down, 0.25f, nullptr, &info_node_id);
        auto faust_tools_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.5f, nullptr, &dockspace_id);
        auto faust_graph_node_id = DockBuilderSplitNode(faust_tools_node_id, ImGuiDir_Left, 0.5f, nullptr, &faust_tools_node_id);
        DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.5f, nullptr, &dockspace_id); // text editor

        Audio.Graph.Dock(audio_node_id);
        Audio.Graph.Connections.Dock(audio_node_id);
        Audio.Style.Dock(audio_node_id);

        Audio.Faust.FaustDsps.Dock(dockspace_id);
        Audio.Faust.Graphs.Dock(faust_graph_node_id);
        Audio.Faust.Paramss.Dock(faust_tools_node_id);
        Audio.Faust.Logs.Dock(faust_tools_node_id);

        Debug.Dock(debug_node_id);
        Debug.StatePreview.Dock(debug_node_id);
        Debug.StorePathUpdateFrequency.Dock(debug_node_id);
        Debug.DebugLog.Dock(debug_node_id);
        Debug.StackTool.Dock(debug_node_id);
        Debug.Metrics.Dock(metrics_node_id);

        Style.Dock(utilities_node_id);
        Demo.Dock(utilities_node_id);

        Info.Dock(info_node_id);
        Settings.Dock(settings_node_id);
    }

    // Draw non-window children.
    for (const auto *child : Children) {
        if (!Windows.IsWindow(child->Id) && child != &Windows) child->Draw();
    }

    Windows.Draw();

    if (frame_count == 1) {
        // Default focused windows.
        Style.Focus();
        Audio.Graph.Focus();
        Audio.Faust.Graphs.Focus();
        Audio.Faust.Paramss.Focus();
        Debug.Focus(); // not visible by default anymore
    }

    if (auto action = ProduceKeyboardAction()) Q(*action);
}

void State::Debug::StorePathUpdateFrequency::Render() const {
    const auto &pc = static_cast<const State &>(*Root).ProjectContext;
    pc.RenderStorePathChangeFrequency();
}

void State::Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void State::Debug::StackTool::Render() const {
    ShowIDStackToolWindow();
}

void State::Debug::Metrics::ImGuiMetrics::Render() const { ImGui::ShowMetricsWindow(); }
void State::Debug::Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

void State::Debug::OnComponentChanged() {
    if (AutoSelect.IsChanged()) {
        WindowFlags = AutoSelect ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None;
    }
}

void State::RenderDebug() const {
    const bool auto_select = Debug.AutoSelect;
    if (auto_select) BeginDisabled();
    RenderValueTree(Debug::LabelModeType(int(Debug.LabelMode)) == Debug::LabelModeType::Annotated, auto_select);
    if (auto_select) EndDisabled();
}

void State::Debug::StatePreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    const auto &pc = static_cast<const State &>(*Root).ProjectContext;
    json project_json = pc.GetProjectJson(ProjectFormat(int(Format)));
    if (Raw) {
        TextUnformatted(project_json.dump(4));
    } else {
        SetNextItemOpen(true);
        fg::JsonTree("", std::move(project_json));
    }
}

void State::Debug::Metrics::Render() const {
    RenderTabs();
}

void State::Debug::Metrics::FlowGridMetrics::Render() const {
    const auto &pc = static_cast<const State &>(*Root).ProjectContext;
    pc.RenderMetrics();
}
