#pragma once

#include "AdjacencyListAction.h"
#include "Core/Action/ActionProducer.h"
#include "Core/Component.h"
#include "Core/ProducerComponentArgs.h"
#include "Core/Store/IdPairs.h"

struct AdjacencyList : Component, ActionProducer<Action::AdjacencyList::Any> {
    using ArgsT = ProducerComponentArgs<ProducedActionType>;
    using Edge = IdPair; // Source, destination

    AdjacencyList(ArgsT &&args) : Component(std::move(args.Args)), ActionProducer(std::move(args.Q)) {
        FieldIds.insert(Id);
    }
    ~AdjacencyList() {
        Erase();
        FieldIds.erase(Id);
    }

    void SetJson(json &&) const override;
    json ToJson() const override;

    IdPairs Get() const;

    void Refresh() override {} // Not cached.
    void RenderValueTree(bool annotate, bool auto_select) const override;

    bool HasPath(ID source, ID destination) const;
    bool IsConnected(ID source, ID destination) const;

    void Add(IdPair &&) const;
    void Connect(ID source, ID destination) const;
    void Disconnect(ID source, ID destination) const;
    void DisconnectOutput(ID id) const;

    u32 SourceCount(ID destination) const;
    u32 DestinationCount(ID source) const;

    bool Exists() const; // Check if exists in store.
    void Erase() const override;
};
