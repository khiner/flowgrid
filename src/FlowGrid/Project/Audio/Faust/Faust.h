#pragma once

#include "FaustAction.h"
#include "FaustBox.h"
#include "FaustDSP.h"
#include "FaustGraphs.h"
#include "FaustParamsUIs.h"

#include "Core/Action/Actionable.h"
#include "Core/Container/Vector.h"

struct FaustDSPs : Vector<FaustDSP> {
    using Vector<FaustDSP>::Vector;

private:
    void Render() const override;
};

struct FaustLogs : Component, FaustChangeListener {
    using Component::Component;

    void OnFaustChanged(const FaustDSP &) override;

    std::vector<std::string> ErrorMessages;

private:
    void Render() const override;
};

struct Faust : Component, Actionable<Action::Faust> {
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
