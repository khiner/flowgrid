#pragma once

#include "FaustAction.h"
#include "FaustDSPs.h"
#include "FaustGraph.h"
#include "FaustGraphStyle.h"
#include "FaustListener.h"
#include "FaustParamsUI.h"
#include "FaustParamsUIStyle.h"

#include "Core/Action/Actionable.h"
#include "Core/Container/Vector.h"

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
