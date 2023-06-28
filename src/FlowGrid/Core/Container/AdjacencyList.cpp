#include "AdjacencyList.h"

#include "Core/Store/Store.h"

void AdjacencyList::Connect(ID source, ID destination) const {
    if (IsConnected(source, destination)) return;

    store::AddIdPair(Path, {source, destination});
}

void AdjacencyList::Disconnect(ID source, ID destination) const {
    store::EraseIdPair(Path, {source, destination});
}

void AdjacencyList::ToggleConnection(ID source, ID destination) const {
    if (IsConnected(source, destination)) Disconnect(source, destination);
    else Connect(source, destination);
}

bool AdjacencyList::IsConnected(ID source, ID destination) const {
    return store::HasIdPair(Path, {source, destination});
}
