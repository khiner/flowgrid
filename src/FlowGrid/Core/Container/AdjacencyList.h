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

    void SetJson(json &&) const override;
    json ToJson() const override;

    IdPairs Get() const;

    // This value is not cached like other fields, because it uses a backing store (a single `immer::set`) that's performant to query.
    void Refresh() override {}
    void RenderValueTree(bool annotate, bool auto_select) const override;

    void Add(IdPair &&) const;
    void Connect(ID source, ID destination) const;
    void Disconnect(ID source, ID destination) const;
    void ToggleConnection(ID source, ID destination) const;
    void DisconnectOutput(ID id) const;
    void Erase() const override;

    bool IsConnected(ID source, ID destination) const;
    bool HasPath(ID source, ID destination) const;
    u32 SourceCount(ID destination) const;
    u32 DestinationCount(ID source) const;
};
