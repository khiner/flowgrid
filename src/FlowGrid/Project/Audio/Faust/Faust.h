#pragma once

#include "FaustAction.h"
#include "FaustDSPListener.h"
#include "FaustGraph.h"
#include "FaustGraphStyle.h"
#include "FaustParams.h"
#include "FaustParamsStyle.h"
#include "Project/Audio/Graph/AudioGraphAction.h"

#include "Core/ActionProducerComponent.h"
#include "Core/ActionableComponent.h"
#include "Core/Container/Vector.h"
#include "Project/TextEditor/TextEditor.h"

struct FileDialog;

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

    FaustParams *FindUi(ID dsp_id) const;

    const FaustParamsStyle &Style;

protected:
    void Render() const override;
};

struct FaustGraphs
    : Vector<FaustGraph>,
      ActionableProducer<Action::Faust::Graph::Any, FaustGraph::ProducedActionType>,
      Component::ChangeListener {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    FaustGraphs(ArgsT &&, const FileDialog &, const FaustGraphStyle &, const FaustGraphSettings &);
    ~FaustGraphs();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    FaustGraph *FindGraph(ID dsp_id) const;

    void OnComponentChanged() override;

    ActionMenuItem<ActionType>
        ShowSaveSvgDialogMenuItem{*this, CreateProducer<ActionType>(), Action::Faust::Graph::ShowSaveSvgDialog{}};

    const FileDialog &FileDialog;
    const FaustGraphStyle &Style;
    const FaustGraphSettings &Settings;

private:
    mutable ID LastSelectedDspId{0};

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

using FaustDspProducedActionType = Action::Append<Action::Combine<Action::Faust::DSP::Any, Action::TextEditor::Any>, typename Action::AudioGraph::CreateFaustNode>;

// `FaustDSP` is a wrapper around a Faust DSP and Box.
// It owns a Faust DSP code buffer, and updates its DSP and Box instances to reflect the current code.
struct FaustDSP : ActionProducerComponent<FaustDspProducedActionType>, Component::ChangeListener {
    FaustDSP(ArgsT &&, FaustDSPContainer &, const FileDialog &);
    ~FaustDSP();

    void OnComponentChanged() override;

    inline static const std::string FaustDspFileExtension = ".dsp";

    FaustDSPContainer &Container;
    const FileDialog &FileDialog;
    ProducerProp_(
        TextEditor, Editor, "Faust code", FileDialog,
        TextEditor::FileConfig{
            {
                .owner_path = Path,
                .title = "Open Faust DSP file",
                .filters = FaustDspFileExtension,
            },
            {
                .owner_path = Path,
                .title = "Save Faust DSP file",
                .filters = FaustDspFileExtension,
                .default_file_name = "my_dsp",
                .save_mode = true,
            },
        },
        fs::path("./res") / "pitch_shifter.dsp",
    );

    Box Box{nullptr};
    dsp *Dsp{nullptr};
    std::string ErrorMessage{""};

private:
    void Render() const override;

    void Init();
    void Uninit();
    void Update(); // Sets `Box`, `Dsp`, and `ErrorMessage` based on the current `Code`.

    void DestroyDsp();

    llvm_dsp_factory *DspFactory{nullptr};
};

struct FaustDSPs
    : Vector<FaustDSP>,
      ActionableProducer<Action::Faust::DSP::Any, FaustDspProducedActionType> {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;

    FaustDSPs(ArgsT &&, const FileDialog &);
    ~FaustDSPs();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

private:
    void Render() const override;
};

struct Faust
    : ActionableComponent<Action::Faust::Any, Action::Append<Action::Combine<Action::Faust::Any, Navigable<ID>::ProducedActionType, Colors::ProducedActionType, TextEditor::ProducedActionType>, Action::AudioGraph::CreateFaustNode>>,
      FaustDSPContainer {
    Faust(ArgsT &&, const FileDialog &);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void RegisterDspChangeListener(FaustDSPListener *listener) const noexcept {
        DspChangeListeners.insert(listener);
        for (auto *faust_dsp : FaustDsps) {
            listener->OnFaustDspAdded(faust_dsp->Id, faust_dsp->Dsp);
        }
    }
    void UnregisterDspChangeListener(FaustDSPListener *listener) const noexcept {
        DspChangeListeners.erase(listener);
    }

    void NotifyListeners(NotificationType type, FaustDSP &faust_dsp) override;

    inline static std::unordered_set<FaustDSPListener *> DspChangeListeners;

    const FileDialog &FileDialog;
    ProducerProp(FaustGraphStyle, GraphStyle);
    Prop(FaustGraphSettings, GraphSettings);
    Prop(FaustParamsStyle, ParamsStyle);

    ProducerProp_(FaustGraphs, Graphs, "Faust graphs", FileDialog, GraphStyle, GraphSettings);
    Prop_(FaustParamss, Paramss, "Faust params", ParamsStyle);
    Prop_(FaustLogs, Logs, "Faust logs");
    ProducerProp(FaustDSPs, FaustDsps, FileDialog);

protected:
    void Render() const override;
};
