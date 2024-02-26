#include "PrimitiveSet.h"

#include "imgui.h"

#include "Core/Store/Store.h"

#include "immer/set_transient.hpp"

template<typename T> bool PrimitiveSet<T>::Exists() const { return RootStore.Contains<ContainerT>(Path); }
template<typename T> void PrimitiveSet<T>::Erase() const { RootStore.Erase<ContainerT>(Path); }

template<typename T> PrimitiveSet<T>::ContainerT PrimitiveSet<T>::Get() const { return RootStore.Get<ContainerT>(Path); }

template<typename T> void PrimitiveSet<T>::Set(const std::set<T> &value) const {
    immer::set_transient<T> val{};
    for (const auto &v : value) val.insert(v);
    RootStore.Set(Path, val.persistent());
}

template<typename T> void PrimitiveSet<T>::Insert(const T &value) const {
    if (!Exists()) RootStore.Set<ContainerT>(Path, {});
    RootStore.Set(Path, Get().insert(value));
}
template<typename T> void PrimitiveSet<T>::Erase_(const T &value) const {
    if (Exists()) RootStore.Set(Path, Get().erase(value));
}
template<typename T> bool PrimitiveSet<T>::Contains(const T &value) const { return Exists() && Get().count(value); }
template<typename T> bool PrimitiveSet<T>::Empty() const { return !Exists() || Get().empty(); }

template<typename T> void PrimitiveSet<T>::SetJson(json &&j) const {
    std::set<T> value = json::parse(std::string(std::move(j)));
    Set(std::move(value));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
template<typename T> json PrimitiveSet<T>::ToJson() const {
    auto value = Get();
    std::set<T> val{};
    for (const auto &v : value) val.insert(v);
    return json(val).dump();
}

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
