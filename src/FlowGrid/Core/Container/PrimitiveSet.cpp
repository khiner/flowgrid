#include "PrimitiveSet.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/set_transient.hpp"

template<typename T> bool PrimitiveSet<T>::Exists() const { return RootStore.Count<ContainerT>(Id); }
template<typename T> void PrimitiveSet<T>::Erase() const { RootStore.Erase<ContainerT>(Id); }
template<typename T> void PrimitiveSet<T>::Clear() const { RootStore.Clear<ContainerT>(Id); }

template<typename T> PrimitiveSet<T>::ContainerT PrimitiveSet<T>::Get() const {
    if (!Exists()) return {};
    return RootStore.Get<ContainerT>(Id);
}

template<typename T> void PrimitiveSet<T>::Insert(const T &value) const {
    if (!Exists()) RootStore.Set<ContainerT>(Id, {});
    RootStore.Set(Id, Get().insert(value));
}
template<typename T> void PrimitiveSet<T>::Erase_(const T &value) const { RootStore.SetErase(Id, value); }

template<typename T> void PrimitiveSet<T>::SetJson(json &&j) const {
    immer::set_transient<T> val{};
    for (const auto &v : json::parse(std::string(std::move(j)))) val.insert(v);
    RootStore.Set(Id, val.persistent());
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json PrimitiveSet<T>::ToJson() const { return json(Get()).dump(); }

using namespace ImGui;

template<typename T> void PrimitiveSet<T>::RenderValueTree(bool annotate, bool auto_select) const {
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
template struct PrimitiveSet<u32>;
