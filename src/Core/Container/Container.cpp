#include "imgui.h"

#include "Core/Primitive/PrimitiveActionQueuer.h"
#include "Core/Store/Store.h"
#include "Project/ProjectContext.h"

#include "Vector.h"

#include "immer/flex_vector_transient.hpp"

using namespace ImGui;

template<typename T> Vector<T>::ContainerT Vector<T>::Get() const { return S.Get<ContainerT>(Id); }
template<typename T> bool Vector<T>::Exists() const { return S.Count<ContainerT>(Id); }
template<typename T> void Vector<T>::Erase() const { _S.Erase<ContainerT>(Id); }
template<typename T> void Vector<T>::Clear() const { _S.Clear<ContainerT>(Id); }

template<typename T> void Vector<T>::Set(const std::vector<T> &value) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : value) val.push_back(v);
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::Set(size_t i, const T &value) const { _S.Set(Id, Get().set(i, value)); }
template<typename T> void Vector<T>::Set(const std::unordered_map<size_t, T> &values) const {
    auto val = Get().transient();
    for (const auto &[i, value] : values) val.set(i, value);
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::PushBack(const T &value) const { _S.Set(Id, S.Get<ContainerT>(Id).push_back(value)); }
template<typename T> void Vector<T>::PopBack() const {
    const auto v = S.Get<ContainerT>(Id);
    _S.Set(Id, v.take(v.size() - 1));
}

template<typename T> void Vector<T>::Resize(size_t size) const {
    auto val = Get().take(size).transient();
    while (val.size() < size) val.push_back(T{});
    _S.Set(Id, val.persistent());
}
template<typename T> void Vector<T>::Erase(size_t i) const { _S.Set(Id, Get().erase(i)); }

template<typename T> size_t Vector<T>::IndexOf(const T &value) const {
    auto vec = Get();
    return std::ranges::find(vec, value) - vec.begin();
}

template<typename T> void Vector<T>::SetJson(json &&j) const {
    immer::flex_vector_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.push_back(v);
    _S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Vector<T>::ToJson() const { return json(Get()).dump(); }

template<typename T> void Vector<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 i = 0; i < value.size(); i++) {
            FlashUpdateRecencyBackground(std::to_string(i));
            TreeNode(std::to_string(i), false, std::format("{}", value[i]).c_str());
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Vector<bool>;
template struct Vector<s32>;
template struct Vector<u32>;
template struct Vector<float>;
template struct Vector<std::string>;

#include "Set.h"

#include "immer/set_transient.hpp"

template<typename T> Set<T>::ContainerT Set<T>::Get() const { return S.Get<ContainerT>(Id); }
template<typename T> bool Set<T>::Exists() const { return S.Count<ContainerT>(Id); }
template<typename T> void Set<T>::Erase() const { _S.Erase<ContainerT>(Id); }
template<typename T> void Set<T>::Clear() const { _S.Clear<ContainerT>(Id); }
template<typename T> void Set<T>::Insert(const T &value) const { _S.Set(Id, Get().insert(value)); }
template<typename T> void Set<T>::Erase(const T &value) const { _S.Set(Id, Get().erase(value)); }

template<typename T> void Set<T>::SetJson(json &&j) const {
    immer::set_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.insert(v);
    _S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Set<T>::ToJson() const { return json(Get()).dump(); }

template<typename T> void Set<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 v : value) {
            FlashUpdateRecencyBackground(std::to_string(v));
            TextUnformatted(std::to_string(v));
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Set<u32>;

#include "Navigable.h"

template<typename T> void Navigable<T>::IssueClear() const { Ctx.PrimitiveQ(typename Action::Navigable<T>::Clear{Id}); }
template<typename T> void Navigable<T>::IssuePush(T value) const { Ctx.PrimitiveQ(typename Action::Navigable<T>::Push{Id, std::move(value)}); }
template<typename T> void Navigable<T>::IssueMoveTo(u32 index) const { Ctx.PrimitiveQ(typename Action::Navigable<T>::MoveTo{Id, index}); }
template<typename T> void Navigable<T>::IssueStepForward() const { Ctx.PrimitiveQ(typename Action::Navigable<T>::MoveTo{Id, u32(Cursor) + 1}); }
template<typename T> void Navigable<T>::IssueStepBackward() const { Ctx.PrimitiveQ(typename Action::Navigable<T>::MoveTo{Id, u32(Cursor) - 1}); }

// Explicit instantiations.
template struct Navigable<u32>;

#include "AdjacencyList.h"

IdPairs AdjacencyList::Get() const { return S.Get<IdPairs>(Id); }

// Non-recursive DFS handling cycles.
bool AdjacencyList::HasPath(ID from_id, ID to_id) const {
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

bool AdjacencyList::Exists() const { return S.Count<IdPairs>(Id); }
bool AdjacencyList::IsConnected(ID source, ID destination) const { return S.Get<IdPairs>(Id).count({source, destination}) > 0; }
void AdjacencyList::Disconnect(ID source, ID destination) const { _S.Set(Id, S.Get<IdPairs>(Id).erase({source, destination})); }
void AdjacencyList::Add(IdPair &&id_pair) const { _S.Set(Id, S.Get<IdPairs>(Id).insert(std::move(id_pair))); }
void AdjacencyList::Connect(ID source, ID destination) const { Add({source, destination}); }

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

void AdjacencyList::Erase() const { _S.Erase<IdPairs>(Id); }

void AdjacencyList::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    const auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name));
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

// Using a string representation to flatten the JSON without worrying about non-object collection values.
json AdjacencyList::ToJson() const { return json(Get()).dump(); }

#include "Vec2.h"

Vec2::Vec2(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Component(std::move(args)), X({this, "X"}, value.first, min, max, fmt), Y({this, "Y"}, value.second, min, max, fmt) {}

Vec2::operator ImVec2() const { return {float(X), float(Y)}; }

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) Ctx.PrimitiveQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, bool linked, const char *fmt)
    : Vec2(std::move(args), std::move(value), min, max, fmt), Linked({this, "Linked"}, linked) {}

Vec2Linked::Vec2Linked(ComponentArgs &&args, std::pair<float, float> &&value, float min, float max, const char *fmt)
    : Vec2Linked(std::move(args), std::move(value), min, max, true, fmt) {}

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    bool linked = Linked;
    if (Checkbox(Linked.Name.c_str(), &linked)) Ctx.PrimitiveQ(Action::Vec2::ToggleLinked{Id});
    PopID();

    SameLine();

    ImVec2 xy = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&xy, X.Min, X.Max, X.Format, flags);
    Component::UpdateGesturing();
    if (edited) {
        if (Linked) {
            Ctx.PrimitiveQ(Action::Vec2::SetAll{Id, xy.x != X ? xy.x : xy.y});
        } else {
            Ctx.PrimitiveQ(Action::Vec2::Set{Id, {xy.x, xy.y}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }
