#pragma once

#include "FaustAction.h"
#include "FaustDSPListener.h"
#include "FaustGraph.h"
#include "FaustGraphStyle.h"
#include "FaustParams.h"
#include "FaustParamsStyle.h"

#include "Core/Action/Actionable.h"
#include "Core/Container/TextBuffer.h"
#include "Core/Container/Vector.h"

/**
- `Audio.Faust.FaustGraphs` (listens to `FaustDSP::Box`): Extensively configurable, live-updating block diagrams for all Faust DSP instances.
  - By default, `FaustGraph` matches the FlowGrid style (which is ImGui's dark style), but it can be configured to exactly match the Faust SVG diagram style.
    `FaustGraph` can also be rendered as an SVG diagram.
    When the graph style is set to the 'Faust' preset, it should look the same as the one produced by `faust2svg` with the same DSP code.
- `Audio.Faust.Params` (listens to `FaustDsp::Dsp`): Interfaces for the params for each Faust DSP instance. TODO: Not undoable yet.
- `Audio.Faust.Logs` (listens to `FaustDSP`, accesses error messages): A window to display Faust compilation errors.

Here is the chain of notifications/updates in response to a Faust DSP code change:
```
Audio.Faust.FaustDsp.Code -> Audio.Faust.FaustDsp
    -> Audio.Faust.FaustGraphs
    -> Audio.Faust.FaustParams
    -> Audio.Faust.FaustLogs
    -> Audio
        -> Audio.Graph.Nodes.Faust
```
**/

struct FaustParamss : Vector<FaustParams> {
    FaustParamss(ComponentArgs &&, const FaustParamsStyle &);

    static std::unique_ptr<FaustParams> CreateChild(Component *, string_view path_prefix_segment, string_view path_segment);

    FaustParams *FindUi(ID dsp_id) const;

    const FaustParamsStyle &Style;

protected:
    void Render() const override;
};

struct FaustGraphs : Vector<FaustGraph>, Actionable<Action::Faust::Graph::Any>, Field::ChangeListener {
    FaustGraphs(ComponentArgs &&, const FaustGraphStyle &, const FaustGraphSettings &);
    ~FaustGraphs();

    static std::unique_ptr<FaustGraph> CreateChild(Component *, string_view path_prefix_segment, string_view path_segment);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    FaustGraph *FindGraph(ID dsp_id) const;
    std::optional<std::string> GetBoxInfo(u32 imgui_id) const {
        for (const auto &graph : *this) {
            if (auto box_info = graph->GetBoxInfo(imgui_id)) {
                return box_info;
            }
        }
        return {};
    }

    static std::optional<std::string> FindBoxInfo(u32 imgui_id);

    void OnFieldChanged() override;

    const FaustGraphStyle &Style;
    const FaustGraphSettings &Settings;

private:
    void Render() const override;
};

struct FaustLogs : Component {
    using Component::Component;

    std::map<ID, std::string> ErrorMessageByFaustDspId;

private:
    void Render() const override;
    void RenderErrorMessage(string_view error_message) const;
};

class llvm_dsp_factory;
enum NotificationType {
    Changed, // Note: This isn't actually used yet. When the DSP changes, we always Remove/Add the FaustDSP.
    Added,
    Removed
};

struct FaustDSP;
struct FaustDSPContainer {
    virtual void NotifyListeners(NotificationType, FaustDSP &) = 0;
};

// `FaustDSP` is a wrapper around a Faust DSP and a Faust Box.
// It owns a Faust DSP code buffer, and updates its DSP and Box instances to reflect the current code.
struct FaustDSP : Component, Field::ChangeListener {
    FaustDSP(ComponentArgs &&, FaustDSPContainer &);
    ~FaustDSP();

    void OnFieldChanged() override;

    Prop_(TextBuffer, Code, "Faust code", R"#(import("stdfaust.lib");
pitchshifter = vgroup("Pitch Shifter", ef.transpose(
   vslider("window (samples)", 1000, 50, 10000, 1),
   vslider("xfade (samples)", 10, 1, 10000, 1),
   vslider("shift (semitones)", 0, -24, +24, 0.1)
 )
);
process = _ : pitchshifter;)#");

    Box Box;
    dsp *Dsp;
    std::string ErrorMessage;

private:
    void Render() const override;

    void Init();
    void Uninit();
    void Update(); // Sets `Box`, `Dsp`, and `ErrorMessage` based on the current `Code`.

    void DestroyDsp();

    FaustDSPContainer &Container;
    llvm_dsp_factory *DspFactory{nullptr};
};

struct FaustDSPs : Vector<FaustDSP>, Actionable<Action::Faust::DSP::Any> {
    FaustDSPs(ComponentArgs &&);
    ~FaustDSPs();

    static std::unique_ptr<FaustDSP> CreateChild(Component *, string_view path_prefix_segment, string_view path_segment);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

private:
    void Render() const override;
};

struct Faust : Component, Actionable<Action::Faust::Any>, FaustDSPContainer {
    Faust(ComponentArgs &&);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    inline void RegisterDspChangeListener(FaustDSPListener *listener) const noexcept {
        DspChangeListeners.insert(listener);
        for (auto *faust_dsp : FaustDsps) {
            listener->OnFaustDspAdded(faust_dsp->Id, faust_dsp->Dsp);
        }
    }
    inline void UnregisterDspChangeListener(FaustDSPListener *listener) const noexcept {
        DspChangeListeners.erase(listener);
    }

    void NotifyListeners(NotificationType type, FaustDSP &faust_dsp) override;

    inline static std::unordered_set<FaustDSPListener *> DspChangeListeners;

    Prop(FaustGraphStyle, GraphStyle);
    Prop(FaustGraphSettings, GraphSettings);
    Prop(FaustParamsStyle, ParamsStyle);

    Prop_(FaustGraphs, Graphs, "Faust graphs", GraphStyle, GraphSettings);
    Prop_(FaustParamss, Paramss, "Faust params", ParamsStyle);
    Prop_(FaustLogs, Logs, "Faust logs");

    Prop(FaustDSPs, FaustDsps);

protected:
    void Render() const override;
};
