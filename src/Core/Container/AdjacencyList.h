#pragma once

#include "Core/Component.h"
#include "Core/Store/IdPairs.h"

struct AdjacencyList : Component {
    using Component::Component;

    using Edge = IdPair; // Source, destination

    ~AdjacencyList() {
        Erase();
    }

    IdPairs Get() const;

    void SetJson(json &&) const override;
    json ToJson() const override;

    void Erase() const override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    bool HasPath(ID source, ID destination) const;
    bool IsConnected(ID source, ID destination) const;

    void Add(IdPair &&) const;
    void Connect(ID source, ID destination) const;
    void Disconnect(ID source, ID destination) const;
    void DisconnectOutput(ID id) const;

    u32 SourceCount(ID destination) const;
    u32 DestinationCount(ID source) const;
};
