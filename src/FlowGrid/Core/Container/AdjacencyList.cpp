#include "AdjacencyList.h"

#include <ranges>
#include <stack>

#include "imgui.h"

#include "Core/Store/Store.h"

IdPairs AdjacencyList::Get() const { return Exists() ? RootStore.Get<IdPairs>(Id) : IdPairs{}; }

bool AdjacencyList::HasPath(ID from_id, ID to_id) const {
    // Non-recursive depth-first search that handles cycles.
    const auto id_pairs = Get();
    std::unordered_set<ID> visited;
    std::stack<ID> to_visit;
    to_visit.push(from_id);
    while (!to_visit.empty()) {
        ID current = to_visit.top();
        to_visit.pop();
        if (current == to_id) return true;

        if (!visited.contains(current)) {
            visited.insert(current);
            for (const auto &[source_id, destination_id] : id_pairs) {
                if (source_id == current) to_visit.push(destination_id);
            }
        }
    }

    return false;
}

bool AdjacencyList::Exists() const { return RootStore.Count<IdPairs>(Id); }

bool AdjacencyList::IsConnected(ID source, ID destination) const {
    return Exists() && RootStore.Get<IdPairs>(Id).count({source, destination}) > 0;
}
void AdjacencyList::Disconnect(ID source, ID destination) const {
    if (Exists()) RootStore.Set(Id, RootStore.Get<IdPairs>(Id).erase({source, destination}));
}
void AdjacencyList::Add(IdPair &&id_pair) const {
    if (!IsConnected(id_pair.first, id_pair.second)) {
        if (!Exists()) RootStore.Set<IdPairs>(Id, {});
        RootStore.Set(Id, RootStore.Get<IdPairs>(Id).insert(std::move(id_pair)));
    }
}
void AdjacencyList::Connect(ID source, ID destination) const { Add({source, destination}); }
void AdjacencyList::ToggleConnection(ID source, ID destination) const {
    if (IsConnected(source, destination)) Disconnect(source, destination);
    else Connect(source, destination);
}
void AdjacencyList::DisconnectOutput(ID id) const {
    for (const auto &[source_id, destination_id] : Get()) {
        if (source_id == id || destination_id == id) Disconnect(source_id, destination_id);
    }
}

u32 AdjacencyList::SourceCount(ID destination) const {
    return std::ranges::count_if(Get(), [destination](const auto &pair) { return pair.second == destination; });
}
u32 AdjacencyList::DestinationCount(ID source) const {
    return std::ranges::count_if(Get(), [source](const auto &pair) { return pair.first == source; });
}

void AdjacencyList::Erase() const { RootStore.Erase<IdPairs>(Id); }

using namespace ImGui;

void AdjacencyList::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    const auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        u32 i = 0;
        for (const auto &v : value) {
            FlashUpdateRecencyBackground(SerializeIdPair(v));
            const auto &[source_id, destination_id] = v;
            const bool can_annotate = annotate && ById.contains(source_id) && ById.contains(destination_id);
            const std::string label = can_annotate ?
                std::format("{} -> {}", ById.at(source_id)->Name, ById.at(destination_id)->Name) :
                std::format("#{:08X} -> #{:08X}", source_id, destination_id);
            TreeNode(std::to_string(i++), false, label.c_str(), can_annotate);
        }
        TreePop();
    }
}

void AdjacencyList::SetJson(json &&j) const {
    Erase();
    for (IdPair id_pair : json::parse(std::string(std::move(j)))) Add(std::move(id_pair));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
json AdjacencyList::ToJson() const { return json(Get()).dump(); }
