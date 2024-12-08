#pragma once

#include "Core/Component.h"
#include "Core/Store/IdPairs.h"

struct AdjacencyList : Component {
    using Component::Component;

    using Edge = IdPair; // Source, destination

    ~AdjacencyList() {
        Erase(_S);
    }

    IdPairs Get() const;

    void SetJson(TransientStore &, json &&) const override;
    json ToJson() const override;

    void Erase(TransientStore &) const override;
    void RenderValueTree(bool annotate, bool auto_select) const override;

    bool HasPath(ID source, ID destination) const;
    bool IsConnected(ID source, ID destination) const;

    u32 SourceCount(ID destination) const;
    u32 DestinationCount(ID source) const;

    void Add(TransientStore &, IdPair &&) const;
    void Connect(TransientStore &, ID source, ID destination) const;
    void Disconnect(TransientStore &, ID source, ID destination) const;
    void DisconnectOutput(TransientStore &, ID id) const;
};
