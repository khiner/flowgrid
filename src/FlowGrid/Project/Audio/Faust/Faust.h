#pragma once

#include "FaustAction.h"
#include "FaustGraph.h"
#include "FaustGraphStyle.h"
#include "FaustListener.h"
#include "FaustParamsUI.h"
#include "FaustParamsUIStyle.h"

#include "Core/Action/Actionable.h"
#include "Core/Container/Vector.h"
#include "Core/Container/TextBuffer.h"

struct FaustParamsUIs : Vector<FaustParamsUI>, FaustDspChangeListener {
    FaustParamsUIs(ComponentArgs &&);

    static std::unique_ptr<FaustParamsUI> CreateChild(Component *, string_view path_prefix_segment, string_view path_segment);

    void OnFaustDspChanged(ID, dsp *) override;
    void OnFaustDspAdded(ID, dsp *) override;
    void OnFaustDspRemoved(ID) override;

    FaustParamsUI *FindUi(ID dsp_id) const;

    Prop(FaustParamsUIStyle, Style);

protected:
    void Render() const override;
};

struct FaustGraphs : Vector<FaustGraph>, Actionable<Action::Faust::Graph::Any>, Field::ChangeListener, FaustBoxChangeListener {
    FaustGraphs(ComponentArgs &&);
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

    void OnFaustBoxChanged(ID, Box) override;
    void OnFaustBoxAdded(ID, Box) override;
    void OnFaustBoxRemoved(ID) override;

    void UpdateNodeImGuiIds() const;

    Prop(FaustGraphSettings, Settings);
    Prop(FaustGraphStyle, Style);

private:
    void Render() const override;
};

struct FaustLogs : Component, FaustChangeListener {
    using Component::Component;

    void OnFaustChanged(ID, const FaustDSP &) override;
    void OnFaustAdded(ID, const FaustDSP &) override;
    void OnFaustRemoved(ID) override;

    std::map<ID, std::string> ErrorMessageByFaustDspId;

private:
    void Render() const override;
    void RenderErrorMessage(string_view error_message) const;
};

class dsp;
class llvm_dsp_factory;

struct FaustDSPs;

// `FaustDSP` is a wrapper around a Faust DSP and a Faust Box.
// It owns a Faust DSP code buffer, and updates its DSP and Box instances to reflect the current code.
struct FaustDSP : Component, Field::ChangeListener {
    FaustDSP(ComponentArgs &&, const FaustDSPContainer &);
    ~FaustDSP();

    void OnFieldChanged() override;

    void Update(); // Sets `Box`, `Dsp`, and `ErrorMessage` based on the current `Code`.

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

    void Init(bool constructing);
    void Uninit(bool destructing);

    void DestroyDsp();

    void NotifyBoxListeners(NotificationType) const;
    void NotifyDspListeners(NotificationType) const;
    void NotifyListeners(NotificationType) const;

    const FaustDSPContainer &Container;
    llvm_dsp_factory *DspFactory{nullptr};
};

struct FaustDSPs : Vector<FaustDSP>, FaustDSPContainer, Actionable<Action::Faust::DSP::Any> {
    FaustDSPs(ComponentArgs &&);
    ~FaustDSPs();

    static std::unique_ptr<FaustDSP> CreateChild(Component *, string_view path_prefix_segment, string_view path_segment);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    inline void RegisterChangeListener(FaustChangeListener *listener) const noexcept {
        ChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustAdded(faust_dsp->Id, *faust_dsp);
        }
    }
    inline void UnregisterChangeListener(FaustChangeListener *listener) const noexcept {
        ChangeListeners.erase(listener);
    }

    inline void RegisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustBoxAdded(faust_dsp->Id, faust_dsp->Box);
        }
    }
    inline void UnregisterBoxChangeListener(FaustBoxChangeListener *listener) const noexcept {
        BoxChangeListeners.erase(listener);
    }

    inline void RegisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.insert(listener);
        for (auto *faust_dsp : *this) {
            listener->OnFaustDspAdded(faust_dsp->Id, faust_dsp->Dsp);
        }
    }
    inline void UnregisterDspChangeListener(FaustDspChangeListener *listener) const noexcept {
        DspChangeListeners.erase(listener);
    }

    inline void NotifyListeners(NotificationType type, const FaustDSP &faust_dsp) const noexcept override {
        for (auto *listener : ChangeListeners) {
            if (type == Changed) listener->OnFaustChanged(faust_dsp.Id, faust_dsp);
            else if (type == Added) listener->OnFaustAdded(faust_dsp.Id, faust_dsp);
            else if (type == Removed) listener->OnFaustRemoved(faust_dsp.Id);
        }
    }
    inline void NotifyBoxListeners(NotificationType type, const FaustDSP &faust_dsp) const noexcept override {
        for (auto *listener : BoxChangeListeners) {
            if (type == Changed) listener->OnFaustBoxChanged(faust_dsp.Id, faust_dsp.Box);
            else if (type == Added) listener->OnFaustBoxAdded(faust_dsp.Id, faust_dsp.Box);
            else if (type == Removed) listener->OnFaustBoxRemoved(faust_dsp.Id);
        }
    }
    inline void NotifyDspListeners(NotificationType type, const FaustDSP &faust_dsp) const noexcept override {
        for (auto *listener : DspChangeListeners) {
            if (type == Changed) listener->OnFaustDspChanged(faust_dsp.Id, faust_dsp.Dsp);
            else if (type == Added) listener->OnFaustDspAdded(faust_dsp.Id, faust_dsp.Dsp);
            else if (type == Removed) listener->OnFaustDspRemoved(faust_dsp.Id);
        }
    }

private:
    void Render() const override;

    inline static std::unordered_set<FaustChangeListener *> ChangeListeners;
    inline static std::unordered_set<FaustBoxChangeListener *> BoxChangeListeners;
    inline static std::unordered_set<FaustDspChangeListener *> DspChangeListeners;
};

struct Faust : Component, Actionable<Action::Faust::Any> {
    Faust(ComponentArgs &&);
    ~Faust();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    Prop(FaustDSPs, FaustDsps);

    Prop_(FaustGraphs, Graphs, "Faust graphs");
    Prop_(FaustParamsUIs, ParamsUis, "Faust params");
    Prop_(FaustLogs, Logs, "Faust logs");

protected:
    void Render() const override;
};
