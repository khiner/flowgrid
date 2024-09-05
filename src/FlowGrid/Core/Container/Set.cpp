#include "Set.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/set_transient.hpp"

template<typename T> bool Set<T>::Exists() const { return S.Count<ContainerT>(Id); }
template<typename T> void Set<T>::Erase() const { S.Erase<ContainerT>(Id); }
template<typename T> void Set<T>::Clear() const { S.Clear<ContainerT>(Id); }

template<typename T> Set<T>::ContainerT Set<T>::Get() const { return S.Get<ContainerT>(Id); }

template<typename T> void Set<T>::Insert(const T &value) const { S.Set(Id, Get().insert(value)); }
template<typename T> void Set<T>::Erase(const T &value) const { S.Set(Id, Get().erase(value)); }

template<typename T> void Set<T>::SetJson(json &&j) const {
    immer::set_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.insert(v);
    S.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json Set<T>::ToJson() const { return json(Get()).dump(); }

using namespace ImGui;

template<typename T> void Set<T>::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    auto value = Get();
    if (value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name, false, nullptr, false, auto_select)) {
        for (u32 v : value) {
            FlashUpdateRecencyBackground(std::to_string(v));
            TextUnformatted(std::to_string(v).c_str());
        }
        TreePop();
    }
}

// Explicit instantiations.
template struct Set<u32>;
