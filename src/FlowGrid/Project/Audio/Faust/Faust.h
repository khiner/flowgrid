#pragma once

#include "FaustAction.h"
#include "FaustDSPs.h"
#include "FaustGraphs.h"
#include "FaustParamsUIs.h"

#include "Core/Action/Actionable.h"

struct FaustLogs : Component, FaustChangeListener {
    using Component::Component;

    void OnFaustChanged(ID, const FaustDSP &) override;
    void OnFaustAdded(ID, const FaustDSP &) override;
    void OnFaustRemoved(ID) override;

    std::vector<std::string> ErrorMessages;

private:
    void Render() const override;
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

extern const Faust &faust;
