#pragma once

#include "AdjacencyListAction.h"
#include "Core/Field/Field.h"
#include "Core/Store/IdPair.h"

struct AdjacencyList : Field, Actionable<Action::AdjacencyList::Any> {
    using Field::Field;

    using Edge = IdPair; // Source, destination

    void Apply(const ActionType &action) const override {
        Visit(
            action,
            [this](const Action::AdjacencyList::ToggleConnection &a) { ToggleConnection(a.source, a.destination); },
        );
    }
    bool CanApply(const ActionType &) const override { return true; }

    // This value is not cached like other fields, because it uses a backing store (a single `immer::set`) that's performant to query.
    void RefreshValue() override {}
    void RenderValueTree(ValueTreeLabelMode, bool auto_select) const override;

    void Connect(ID source, ID destination) const;
    void Disconnect(ID source, ID destination) const;
    void ToggleConnection(ID source, ID destination) const;
    bool IsConnected(ID source, ID destination) const;
};
